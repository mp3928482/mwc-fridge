#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ota.h"
#include "logger.h"

// ── WiFi connection ───────────────────────────────────────────────────────────
static bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to '%s'", cfg.wifi_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("\n[WiFi] ERROR: Could not connect. Check wifi_ssid and wifi_password in config.json");
    return false;
}

// ── Track when we last ran OTA check ─────────────────────────────────────────
static unsigned long lastOTACheckMs    = 0;
static unsigned long lastLogMs         = 0;

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================");
    Serial.printf("  MWC Fridge Monitor  v%s\n", FIRMWARE_VERSION);
    Serial.println("========================================");

    // 1. Load local secrets from LittleFS
    if (!loadLocalConfig()) {
        Serial.println("[FATAL] Cannot load local config. Halting.");
        while (true) delay(5000);   // halt — blink LED here if desired
    }

    // 2. Connect to WiFi
    if (!connectWiFi()) {
        Serial.println("[FATAL] Cannot connect to WiFi. Halting.");
        while (true) delay(5000);
    }

    // 3. Fetch remote config (non-fatal if offline after WiFi connected)
    loadRemoteConfig();

    // 4. Check for OTA firmware update
    checkAndApplyOTA();
    lastOTACheckMs = millis();

    // 5. Init temperature sensor
    initSensor();

    // 6. Log immediately on first boot
    readAndLog();
    lastLogMs = millis();

    Serial.printf("[Main] Setup complete. Logging every %d min, OTA check every %d hr.\n",
                  cfg.log_interval_min, cfg.ota_check_hours);
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    unsigned long now = millis();

    // ── Temperature logging ───────────────────────────────────────────────
    unsigned long logIntervalMs = (unsigned long)cfg.log_interval_min * 60UL * 1000UL;
    if (now - lastLogMs >= logIntervalMs) {
        // Reconnect WiFi if dropped
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Main] WiFi dropped — reconnecting...");
            connectWiFi();
        }
        readAndLog();
        lastLogMs = millis();
    }

    // ── OTA check ─────────────────────────────────────────────────────────
    unsigned long otaIntervalMs = (unsigned long)cfg.ota_check_hours * 3600UL * 1000UL;
    if (now - lastOTACheckMs >= otaIntervalMs) {
        checkAndApplyOTA();
        lastOTACheckMs = millis();
    }

    // Small yield to keep watchdog happy
    delay(1000);
}
