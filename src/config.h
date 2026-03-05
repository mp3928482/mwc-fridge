#pragma once
#include <Arduino.h>

// ── Hardcoded lifelines (never in config files) ─────────────────────────────
// These MUST stay in firmware — they are the recovery mechanism if anything
// else goes wrong. Changing them requires a USB flash.
#define FIRMWARE_VERSION     "1.0.0"
#define GITHUB_RAW_BASE      "https://raw.githubusercontent.com/mp3928482/mwc-fridge/main/releases"
#define GITHUB_VERSION_URL   GITHUB_RAW_BASE "/version.txt"
#define GITHUB_RCONFIG_URL   GITHUB_RAW_BASE "/remote_config.json"
// Firmware .bin is served from GitHub Releases (not raw), built dynamically in ota.cpp

// ── WiFi network entry ───────────────────────────────────────────────────────
struct WifiNetwork {
    String ssid;
    String password;
};

// ── Merged config struct (local + remote combined) ──────────────────────────
struct AppConfig {
    // From local config.json (secrets)
    WifiNetwork wifi_networks[3];
    int         wifi_count;
    String      fridge_id;

    // From remote_config.json (tunables)
    String sheet_url;
    int    log_interval_min;
    int    ota_check_hours;
    float  alert_min_f;
    float  alert_max_f;
};

// Populated by loadConfig() — use everywhere
extern AppConfig cfg;

// ── Function declarations ────────────────────────────────────────────────────

// Mount LittleFS, read local config.json into cfg.
// Returns false and halts if local config is missing or malformed.
bool loadLocalConfig();

// Fetch remote_config.json from GitHub and merge into cfg.
// Non-fatal if fetch fails — keeps sensible defaults.
void loadRemoteConfig();
