#include "logger.h"
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

static OneWire           oneWire(ONE_WIRE_BUS);
static DallasTemperature sensors(&oneWire);

// Track alert state to avoid spamming — only alert on transition into out-of-range
static bool wasOutOfRange = false;

// ── Sensor init ───────────────────────────────────────────────────────────────
void initSensor() {
    sensors.begin();
    delay(500);    // allow DS18B20 to stabilize after power-up
    int count = sensors.getDeviceCount();
    Serial.printf("[Sensor] DS18B20 init — %d device(s) found on GPIO %d\n", count, ONE_WIRE_BUS);
    if (count == 0) {
        Serial.println("[Sensor] WARNING: No sensors found. Check wiring and 4.7k pull-up resistor.");
    }
}

// ── Post a row to Google Sheets via Apps Script web app ──────────────────────
static void postToSheet(float tempF, float tempC, const String& status) {
    if (cfg.sheet_url.isEmpty()) {
        Serial.println("[Logger] sheet_url not set — skipping POST");
        return;
    }

    // Build query string
    String url = cfg.sheet_url
               + "?fridge="  + cfg.fridge_id
               + "&tempF="   + String(tempF, 2)
               + "&tempC="   + String(tempC, 2)
               + "&status="  + status
               + "&version=" + FIRMWARE_VERSION;

    WiFiClientSecure client;
    client.setInsecure();   // Google Script redirects through HTTPS — setInsecure is fine here

    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // Apps Script issues a redirect
    http.setTimeout(15000);

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        Serial.printf("[Logger] Sheet POST OK — fridge: %s, %.2f°F / %.2f°C [%s]\n",
                      cfg.fridge_id.c_str(), tempF, tempC, status.c_str());
    } else {
        Serial.printf("[Logger] Sheet POST failed (HTTP %d)\n", code);
    }
    http.end();
}

// ── Main read-and-log function ────────────────────────────────────────────────
void readAndLog() {
    // Sanity check WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Logger] WiFi not connected — skipping log");
        return;
    }

    // Request temperature with retry — noisy power environments can cause
    // spurious 185°F readings. Retry up to 3 times before giving up.
    float tempC = DEVICE_DISCONNECTED_C;
    for (int attempt = 1; attempt <= 3; attempt++) {
        sensors.requestTemperatures();
        delay(750);   // DS18B20 needs up to 750ms for 12-bit conversion
        tempC = sensors.getTempCByIndex(0);

        if (tempC != DEVICE_DISCONNECTED_C && tempC > -55.0 && tempC < 125.0) {
            if (attempt > 1) {
                Serial.printf("[Sensor] Good reading on attempt %d\n", attempt);
            }
            break;
        }
        Serial.printf("[Sensor] Bad reading attempt %d (%.2f°C) — retrying...\n", attempt, tempC);
        delay(500);
    }

    // If all 3 attempts failed, log a sensor error
    if (tempC == DEVICE_DISCONNECTED_C || tempC < -55.0 || tempC > 125.0) {
        Serial.println("[Logger] ERROR: Sensor read failed after 3 attempts — check wiring");
        postToSheet(0, 0, "SENSOR_ERROR");
        return;
    }

    float tempF = (tempC * 9.0f / 5.0f) + 32.0f;

    // ── Alert logic ───────────────────────────────────────────────────────
    bool outOfRange = (tempF < cfg.alert_min_f || tempF > cfg.alert_max_f);
    String status   = "OK";

    if (tempF < cfg.alert_min_f) {
        status = "ALERT_LOW";
        Serial.printf("[Logger] *** ALERT: Temp %.2f°F is BELOW minimum %.2f°F ***\n",
                      tempF, cfg.alert_min_f);
    } else if (tempF > cfg.alert_max_f) {
        status = "ALERT_HIGH";
        Serial.printf("[Logger] *** ALERT: Temp %.2f°F is ABOVE maximum %.2f°F ***\n",
                      tempF, cfg.alert_max_f);
    }

    // Only log transition into out-of-range to Serial (reduces noise)
    if (outOfRange && !wasOutOfRange) {
        Serial.println("[Logger] Transitioning to OUT-OF-RANGE state");
    } else if (!outOfRange && wasOutOfRange) {
        Serial.println("[Logger] Temperature back in normal range");
    }
    wasOutOfRange = outOfRange;

    // Always post to sheet — Apps Script handles email alerts based on status field
    postToSheet(tempF, tempC, status);
}
