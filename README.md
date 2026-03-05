# MWC Fridge Temperature Monitor

An ESP32-based refrigerator temperature monitoring system that logs readings to Google Sheets and sends email alerts when temperatures go out of range. Supports over-the-air (OTA) firmware updates via GitHub Releases.

---

## How It Works

```
DS18B20 Probe  →  ESP32  →  WiFi  →  Google Apps Script  →  Google Sheets
                                                          →  Email Alerts
```

On each boot and at a configurable interval, the ESP32:
1. Reads the temperature from a DS18B20 probe
2. POSTs the reading to a Google Apps Script web app
3. The script appends a row to Google Sheets and sends an alert email if the temp is out of range

Additionally, on each boot and every N hours, the ESP32 checks GitHub for a newer firmware version and self-updates if one is available.

---

## Repository Structure

```
mwc-fridge/
│
├── data/
│   ├── config.json              ← secrets (gitignored — never committed)
│   ├── config.json.template     ← template showing required fields
│   └── config-fridge-X.json    ← per-fridge configs (gitignored)
│
├── releases/
│   ├── remote_config.json       ← non-secret tunables (sheet URL, intervals, thresholds)
│   ├── version.txt              ← current firmware version number
│   └── Code.gs                  ← Google Apps Script source
│
├── src/
│   ├── main.cpp                 ← boot sequence and main loop
│   ├── config.h / config.cpp    ← config loader (local + remote merge)
│   ├── ota.h / ota.cpp          ← GitHub OTA update checker
│   └── logger.h / logger.cpp    ← DS18B20 reader and Sheets POST
│
├── platformio.ini               ← PlatformIO build config
└── .gitignore                   ← excludes all config.json files
```

---

## Hardware

### Components (per fridge unit)

| Component | Description | Approx Cost |
|---|---|---|
| ESP32-WROOM-32 | Dev board | $6 |
| DS18B20 | Waterproof temperature probe | $3 |
| 4.7kΩ resistor | Pull-up for OneWire bus | <$1 |
| USB power supply | 5V phone charger | $5 |
| Project box | To mount the ESP32 | $2 |

### Wiring

```
DS18B20          ESP32-WROOM-32
─────────        ──────────────
Red   (VCC)  →   3V3
Black (GND)  →   GND
Yellow (DATA) →  D4 (GPIO4)

4.7kΩ resistor between DATA and 3V3  ← required pull-up
```

> **Important:** The ESP32-WROOM-32 only supports 2.4GHz WiFi — 5GHz networks will not work.

---

## Configuration

### What Goes Where

| Parameter | Location | Reason |
|---|---|---|
| `wifi_networks` | `data/config.json` | Per-device, secret |
| `fridge_id` | `data/config.json` | Unique per unit |
| `sheet_url` | `releases/remote_config.json` | Remotely updatable |
| `log_interval_min` | `releases/remote_config.json` | Remotely updatable |
| `ota_check_hours` | `releases/remote_config.json` | Remotely updatable |
| `alert_min_f` | `releases/remote_config.json` | Remotely updatable |
| `alert_max_f` | `releases/remote_config.json` | Remotely updatable |
| `FIRMWARE_VERSION` | `src/config.h` | Baked into firmware |
| `GITHUB_OTA_BASE_URL` | `src/config.h` | Hardcoded lifeline |

### Local Config (`data/config.json`)

This file is **never committed to GitHub**. It contains WiFi credentials and the fridge identifier for each unit. Create it by copying `data/config.json.template`:

```json
{
  "wifi_networks": [
    { "ssid": "PrimaryNetwork",  "password": "PrimaryPassword" },
    { "ssid": "BackupNetwork",   "password": "BackupPassword" }
  ],
  "fridge_id": "Fridge-1"
}
```

Up to 3 networks can be listed. The device tries each in order and connects to the first available. Only one entry is required.

### Remote Config (`releases/remote_config.json`)

This file lives in the GitHub repo and is fetched by the ESP32 on every boot. Update it and push to GitHub to change settings on all devices without reflashing:

```json
{
  "sheet_url":        "https://script.google.com/macros/s/YOUR_SCRIPT_ID/exec",
  "log_interval_min": 30,
  "ota_check_hours":  6,
  "alert_min_f":      32.0,
  "alert_max_f":      40.0
}
```

### Hardcoded in Firmware (`src/config.h`)

```cpp
#define FIRMWARE_VERSION   "1.0.0"
#define GITHUB_RAW_BASE    "https://raw.githubusercontent.com/mp3928482/mwc-fridge/main/releases"
```

These are the OTA recovery lifelines — changing them requires a USB flash.

---

## Boot Sequence

```
Power on
  │
  ├─ Mount LittleFS
  ├─ Read data/config.json  (wifi creds, fridge ID)
  ├─ Connect to WiFi
  │
  ├─ Fetch releases/remote_config.json from GitHub
  │     └─ merge with local config
  │
  ├─ Check releases/version.txt from GitHub
  │     ├─ newer? → download firmware.bin → flash → reboot
  │     └─ same?  → continue
  │
  └─ Enter logging loop
        ├─ Read DS18B20 temp
        ├─ POST to Google Sheet
        ├─ Wait log_interval_min
        └─ repeat
```

Expected Serial Monitor output on a clean boot:
```
========================================
  MWC Fridge Monitor  v1.0.0
========================================
[Config] Mounting LittleFS...
[Config] Local config OK — fridge_id: Fridge-1
[WiFi] Trying 1/2: 'PrimaryNetwork'..........
[WiFi] Connected to 'PrimaryNetwork' — IP: 192.168.1.xxx
[Config] Remote config OK — sheet_url set, interval: 30min, OTA every: 6h
[OTA] Firmware is up to date.
[Sensor] DS18B20 init — 1 device(s) found on GPIO 4
[Logger] Sheet POST OK — fridge: Fridge-1, 37.50°F / 3.06°C [OK]
[Main] Setup complete. Logging every 30 min, OTA check every 6h.
```

---

## First-Time Device Setup (USB Required Once)

1. **Install PlatformIO** extension in VS Code

2. **Create `data/config.json`** for this specific unit:
   ```json
   {
     "wifi_networks": [
       { "ssid": "NetworkName", "password": "Password" }
     ],
     "fridge_id": "Fridge-1"
   }
   ```

3. **Upload the filesystem** (sends config.json to LittleFS):
   PlatformIO sidebar → esp32dev → Platform → **Upload Filesystem Image**

4. **Flash the firmware**:
   Click the **→ Upload** button in the PlatformIO toolbar

5. **Verify via Serial Monitor** (plug icon, 115200 baud):
   Confirm clean boot output as shown above

6. **No USB needed again** — all future updates are OTA via GitHub

### Per-Fridge Config Files

Keep a named config file for each fridge unit in `data/`. All are gitignored:

```
data/config-fridge-1.json    ← Kitchen fridge
data/config-fridge-2.json    ← Storage fridge
data/config-fridge-3.json    ← Freezer
```

To flash a specific unit:
1. Copy the appropriate file: rename it to `config.json`
2. Run **Upload Filesystem Image**
3. Run **Upload** (firmware)

---

## Google Apps Script Setup

1. Create a new Google Sheet at sheets.google.com
2. Copy the Spreadsheet ID from the URL:
   `https://docs.google.com/spreadsheets/d/**SPREADSHEET_ID**/edit`
3. Open **Extensions → Apps Script**
4. Delete the default code and paste the contents of `releases/Code.gs`
5. Update the two config lines at the top:
   ```javascript
   var SPREADSHEET_ID = "your-spreadsheet-id";
   var ALERT_EMAIL    = "alerts@youremail.com";
   ```
6. Click **Deploy → New deployment**
   - Type: **Web app**
   - Execute as: **Me**
   - Who has access: **Anyone**
7. Copy the deployment URL and paste it into `releases/remote_config.json` as `sheet_url`
8. Commit and push `remote_config.json`

### Google Sheet Columns

| Column | Contents |
|---|---|
| A | Timestamp |
| B | Fridge ID |
| C | Temp (°F) |
| D | Temp (°C) |
| E | Status (OK / ALERT_HIGH / ALERT_LOW / SENSOR_ERROR) |
| F | Firmware version |

Status cells are color coded — green for OK, red for alerts, yellow for sensor errors.

### Alert Email Cooldown

To prevent alert spam, emails are suppressed if an alert was already sent for the same fridge within 60 minutes. To manually reset the cooldown, open the Apps Script editor and run the `clearAlertCooldown()` function.

---

## Releasing a Firmware Update (OTA)

All devices self-update within `ota_check_hours` of a new release being published.

1. Make code changes in VS Code
2. Bump the version in **two places**:
   - `src/config.h` → `#define FIRMWARE_VERSION "1.0.1"`
   - `releases/version.txt` → `1.0.1`
3. Build: PlatformIO **✓ Build** button
4. Firmware binary location: `.pio/build/esp32dev/firmware.bin`
5. Commit and push all changes
6. On GitHub: **Releases → Create a new release**
   - Tag: `v1.0.1`
   - Attach these files to the release assets:
     - `.pio/build/esp32dev/firmware.bin`
     - `releases/version.txt`
     - `releases/remote_config.json`
7. Click **Publish release**

> **Note:** The ESP32 downloads `firmware.bin` from GitHub Releases (not raw). The `releases/` folder in the repo is for remote config and version tracking — the actual binary goes in the GitHub Release assets.

---

## Updating Remote Config Without a Firmware Flash

To change sheet URL, logging interval, or alert thresholds without touching any device:

1. Edit `releases/remote_config.json` in VS Code
2. Commit and push
3. All devices pick up the new values on next boot

---

## Troubleshooting

| Serial output | Cause | Fix |
|---|---|---|
| `LittleFS mount failed` | Filesystem not uploaded | Re-run Upload Filesystem Image |
| `config.json not found` | File in wrong folder | Make sure it's in `data/` not `src/` |
| `No sensors found` | Wiring issue | Check pull-up resistor and DATA pin |
| `All networks failed` | Wrong credentials or out of range | Check SSIDs/passwords in config.json — case sensitive, 2.4GHz only |
| `Sheet POST failed` | Wrong URL or script not deployed | Check sheet_url in remote_config.json |
| `-127°F reading` | Sensor disconnected | Check solder joints on DATA pin |
| OTA not triggering | Version numbers match | Ensure version.txt and config.h are both bumped |

---

## Dependencies

### PlatformIO Libraries (`platformio.ini`)

| Library | Purpose |
|---|---|
| `paulstoffregen/OneWire` | OneWire bus communication |
| `milesburton/DallasTemperature` | DS18B20 sensor wrapper |
| `bblanchon/ArduinoJson` | JSON parsing for config files |
| ESP32 Arduino core | WiFi, HTTPClient, Update, LittleFS |

### Tools

- [VS Code](https://code.visualstudio.com/) with [PlatformIO extension](https://platformio.org/)
- [Git](https://git-scm.com/)
- Google Account (Sheets + Apps Script)
- GitHub account

---

## Security Notes

- `data/config.json` and all `data/config-*.json` files are gitignored and must **never** be committed
- The GitHub repo is public (required for free raw file access) — ensure no secrets are ever pushed
- The Google Apps Script web app URL is effectively a write-only API key — treat it as semi-sensitive but it is safe to store in the public repo since it can only append rows to your sheet
- OTA updates use `setInsecure()` for HTTPS — acceptable for this use case but be aware it skips certificate validation
