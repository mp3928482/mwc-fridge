# Claude Code Context — MWC Fridge Monitor

## Project Purpose
ESP32-based refrigerator temperature monitoring system built for a non-profit
organization. Monitors fridge temps, logs readings to Google Sheets, and sends
email alerts when temperatures go out of safe range. Designed for zero-touch
operation after initial setup — all updates are delivered OTA via GitHub.

---

## Technology Stack

- **Hardware:** ESP32-WROOM-32 + DS18B20 waterproof temperature probe
- **Framework:** Arduino via PlatformIO (not ESP-IDF)
- **IDE:** VS Code with PlatformIO extension
- **Filesystem:** LittleFS for local config storage on device
- **OTA:** GitHub Releases — device polls version.txt and self-flashes firmware.bin
- **Backend:** Google Apps Script web app (doGet handler)
- **Data store:** Google Sheets (one row per reading)
- **Alerts:** Gmail via Apps Script MailApp

---

## Repository Structure

```
mwc-fridge/
├── data/
│   ├── config.json              ← GITIGNORED — secrets, never commit
│   ├── config.json.template     ← safe template showing required fields
│   └── config-fridge-X.json    ← GITIGNORED — per-unit configs
├── releases/
│   ├── remote_config.json       ← tunables fetched by ESP32 at boot
│   ├── version.txt              ← current firmware version (e.g. "1.0.0")
│   └── Code.gs                  ← Google Apps Script source
├── src/
│   ├── main.cpp                 ← setup() and loop() only — orchestration
│   ├── config.h / config.cpp    ← AppConfig struct, local + remote loader
│   ├── ota.h / ota.cpp          ← GitHub version check and OTA flash
│   └── logger.h / logger.cpp    ← DS18B20 read and Sheets HTTP POST
├── platformio.ini
├── .gitignore
├── README.md
└── CLAUDE.md                    ← this file
```

---

## Hardware Details

- **Board:** ESP32-WROOM-32
- **Sensor:** DS18B20 waterproof probe on GPIO4 (labeled D4 on board)
- **Pull-up:** 4.7kΩ resistor between DATA and 3.3V (required for OneWire)
- **Power:** 5V USB supply into ESP32 dev board
- **WiFi:** 2.4GHz only — ESP32-WROOM-32 does not support 5GHz

Wiring:
```
DS18B20 Red   (VCC)  → 3V3
DS18B20 Black (GND)  → GND
DS18B20 Yellow (DATA) → D4 (GPIO4)
4.7kΩ resistor        → between D4 and 3V3
```

---

## Config Architecture

There are three layers of configuration:

### 1. Hardcoded in firmware (src/config.h)
```cpp
#define FIRMWARE_VERSION   "1.0.0"   // bump this for every release
#define GITHUB_RAW_BASE    "https://raw.githubusercontent.com/mp3928482/mwc-fridge/main/releases"
#define GITHUB_VERSION_URL GITHUB_RAW_BASE "/version.txt"
#define GITHUB_RCONFIG_URL GITHUB_RAW_BASE "/remote_config.json"
```
These are the OTA recovery lifelines. If these are wrong the device cannot
self-recover. Only changeable via USB flash.

### 2. Local secrets — data/config.json (LittleFS, USB upload once)
```json
{
  "wifi_networks": [
    { "ssid": "PrimaryNetwork",  "password": "PrimaryPassword" },
    { "ssid": "BackupNetwork",   "password": "BackupPassword" }
  ],
  "fridge_id": "Fridge-1"
}
```
NEVER committed to GitHub. Uploaded to device via PlatformIO
"Upload Filesystem Image". Each unit has a unique fridge_id.
Up to 3 networks supported — device tries each in order at boot.

### 3. Remote tunables — releases/remote_config.json (GitHub, fetched at boot)
```json
{
  "sheet_url":        "https://script.google.com/macros/s/SCRIPT_ID/exec",
  "log_interval_min": 30,
  "ota_check_hours":  6,
  "alert_min_f":      32.0,
  "alert_max_f":      40.0
}
```
Committed to GitHub. Changes here are picked up by all devices on next boot
without any reflashing. This is the preferred way to update settings.

---

## AppConfig Struct (src/config.h)

```cpp
struct WifiNetwork {
    String ssid;
    String password;
};

struct AppConfig {
    // From local config.json
    WifiNetwork wifi_networks[3];
    int         wifi_count;
    String      fridge_id;

    // From remote_config.json
    String sheet_url;
    int    log_interval_min;
    int    ota_check_hours;
    float  alert_min_f;
    float  alert_max_f;
};
extern AppConfig cfg;   // global, populated at boot
```

---

## Boot Sequence (main.cpp)

```
loadLocalConfig()       → mount LittleFS, parse config.json
connectWiFi()           → try each cfg.wifi_networks[] entry in order until one connects
loadRemoteConfig()      → fetch remote_config.json from GitHub, merge into cfg
checkAndApplyOTA()      → fetch version.txt, compare, flash if newer
initSensor()            → DS18B20 init on GPIO4
readAndLog()            → first reading immediately on boot
loop()                  → readAndLog() every log_interval_min
                          checkAndApplyOTA() every ota_check_hours
```

---

## Google Sheets Integration

- ESP32 sends an HTTP GET to the Apps Script web app URL with query parameters
- Apps Script appends a row and sends alert email if status is ALERT_HIGH/ALERT_LOW
- Sheet columns: Timestamp | Fridge ID | Temp °F | Temp °C | Status | Firmware
- Status values: OK | ALERT_HIGH | ALERT_LOW | SENSOR_ERROR
- Alert email cooldown: 60 minutes per fridge (stored in Apps Script Properties)

HTTP GET format:
```
{sheet_url}?fridge=Fridge-1&tempF=37.50&tempC=3.06&status=OK&version=1.0.0
```

---

## OTA Update Process

The device checks GitHub for a newer firmware version and self-flashes:

1. Fetches `releases/version.txt` from GitHub raw URL
2. Compares remote version to `FIRMWARE_VERSION` using semver (major.minor.patch)
3. If remote is newer: downloads firmware.bin from GitHub Releases (not raw)
4. Flashes via `httpUpdate.update()` and reboots automatically
5. If OTA fails: device continues running current firmware (safe fallback)

Firmware binary URL pattern:
```
https://github.com/mp3928482/mwc-fridge/releases/latest/download/firmware.bin
```

---

## Release Checklist

When pushing a firmware update, always do ALL of these:

- [ ] Bump `FIRMWARE_VERSION` in `src/config.h`
- [ ] Bump version number in `releases/version.txt` (must match exactly)
- [ ] Build firmware: PlatformIO ✓ Build button
- [ ] Commit and push all changes
- [ ] Create GitHub Release tagged `vX.Y.Z`
- [ ] Attach to release assets: `firmware.bin`, `version.txt`, `remote_config.json`

If version.txt and FIRMWARE_VERSION don't match, OTA will loop or never trigger.

---

## Per-Device Flash Checklist (first time, USB required)

- [ ] Create `data/config.json` with correct wifi_networks array and fridge_id
- [ ] PlatformIO → Upload Filesystem Image  (uploads config.json to LittleFS)
- [ ] PlatformIO → Upload  (flashes firmware)
- [ ] Open Serial Monitor at 115200 baud and verify clean boot
- [ ] Confirm new row appears in Google Sheet
- [ ] No USB needed again after this

---

## Critical Rules — Always Follow These

1. **Never suggest committing `data/config.json` or `data/config-*.json`** — these contain WiFi passwords
2. **Always bump FIRMWARE_VERSION and version.txt together** — mismatches break OTA
3. **The repo must stay public** — ESP32 fetches raw files from GitHub without auth
4. **sheet_url goes in remote_config.json, not config.json** — it's not a secret and needs to be remotely updatable
5. **GITHUB_RAW_BASE and GITHUB_OTA_BASE_URL are the recovery lifelines** — changing them requires a USB flash of all devices
6. **Don't add 5GHz-only WiFi** — ESP32-WROOM-32 is 2.4GHz only
7. **LittleFS not SPIFFS** — platformio.ini is configured for LittleFS
8. **Follow redirects on HTTP POST** — Google Apps Script issues a redirect, requires `HTTPC_STRICT_FOLLOW_REDIRECTS`

---

## Common Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| `config.json not found` | Wrong folder | Must be in `data/` not `src/` |
| `No sensors found` | Missing pull-up | 4.7kΩ between D4 and 3V3 |
| `-127°F` reading | Sensor disconnected | Check solder on DATA pin |
| WiFi fails | 5GHz network | Use 2.4GHz network |
| Sheet POST fails | Missing redirect follow | Check `setFollowRedirects` in logger.cpp |
| OTA loops forever | Version mismatch | Ensure config.h and version.txt match |
| Alert spam | Cooldown not set | Run `clearAlertCooldown()` in Apps Script |

---

## PlatformIO Libraries

```ini
lib_deps =
    paulstoffregen/OneWire @ ^2.3.7
    milesburton/DallasTemperature @ ^3.11.0
    bblanchon/ArduinoJson @ ^7.0.4
```

WiFi, HTTPClient, HTTPUpdate, Update, and LittleFS are all part of the
ESP32 Arduino core and do not need separate installation.
