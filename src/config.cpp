#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Global config instance
AppConfig cfg;

// ── Load local config.json from LittleFS ────────────────────────────────────
bool loadLocalConfig() {
    Serial.println("[Config] Mounting LittleFS...");

    if (!LittleFS.begin(true)) {   // true = format if mount fails
        Serial.println("[Config] ERROR: LittleFS mount failed");
        return false;
    }

    if (!LittleFS.exists("/config.json")) {
        Serial.println("[Config] ERROR: /config.json not found.");
        Serial.println("[Config] Upload data/config.json via PlatformIO 'Upload Filesystem Image'");
        return false;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("[Config] ERROR: Cannot open /config.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Config] ERROR: JSON parse failed: %s\n", err.c_str());
        return false;
    }

    // Required fields — halt if missing
    if (!doc["fridge_id"]) {
        Serial.println("[Config] ERROR: config.json missing required field: fridge_id");
        return false;
    }

    cfg.wifi_count = 0;
    JsonArray networks = doc["wifi_networks"].as<JsonArray>();
    for (JsonObject net : networks) {
        if (cfg.wifi_count >= 3) break;
        cfg.wifi_networks[cfg.wifi_count].ssid     = net["ssid"].as<String>();
        cfg.wifi_networks[cfg.wifi_count].password = net["password"].as<String>();
        cfg.wifi_count++;
    }
    if (cfg.wifi_count == 0) {
        Serial.println("[Config] ERROR: config.json missing wifi_networks (need at least one entry)");
        return false;
    }

    cfg.fridge_id = doc["fridge_id"].as<String>();

    // Sensible defaults for remote tunables (overwritten by loadRemoteConfig)
    cfg.sheet_url        = "";
    cfg.log_interval_min = 30;
    cfg.ota_check_hours  = 6;
    cfg.alert_min_f      = 32.0;
    cfg.alert_max_f      = 40.0;

    Serial.printf("[Config] Local config OK — fridge_id: %s\n", cfg.fridge_id.c_str());
    return true;
}

// ── Fetch remote_config.json from GitHub and merge into cfg ─────────────────
void loadRemoteConfig() {
    Serial.println("[Config] Fetching remote_config.json from GitHub...");

    WiFiClientSecure client;
    client.setInsecure();   // GitHub raw content — acceptable for config fetch

    HTTPClient http;
    http.begin(client, GITHUB_RCONFIG_URL);
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Config] WARNING: remote_config fetch failed (HTTP %d) — using defaults\n", code);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Config] WARNING: remote_config parse failed: %s — using defaults\n", err.c_str());
        return;
    }

    // Merge — only overwrite fields that are present
    if (doc["sheet_url"])        cfg.sheet_url        = doc["sheet_url"].as<String>();
    if (doc["log_interval_min"]) cfg.log_interval_min = doc["log_interval_min"].as<int>();
    if (doc["ota_check_hours"])  cfg.ota_check_hours  = doc["ota_check_hours"].as<int>();
    if (doc["alert_min_f"])      cfg.alert_min_f      = doc["alert_min_f"].as<float>();
    if (doc["alert_max_f"])      cfg.alert_max_f      = doc["alert_max_f"].as<float>();

    Serial.printf("[Config] Remote config OK — sheet_url set, interval: %dmin, OTA every: %dh\n",
                  cfg.log_interval_min, cfg.ota_check_hours);
}
