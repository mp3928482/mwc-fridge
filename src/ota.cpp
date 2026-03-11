#include "ota.h"
#include "config.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

// GitHub Releases download URL pattern for firmware binary.
// Constructed at runtime so the base URL stays in the hardcoded GITHUB_RAW_BASE.
// Format: https://github.com/<user>/<repo>/releases/latest/download/firmware.bin
static const char* FIRMWARE_BIN_URL =
    "https://github.com/mp3928482/mwc-fridge/releases/latest/download/firmware.bin";

// ── Helpers ──────────────────────────────────────────────────────────────────

// Trim whitespace/newlines from a string (version.txt often has a trailing \n)
static String trim(String s) {
    s.trim();
    return s;
}

// Simple semver comparison: returns true if remote > local
// Expects strings like "1.2.3"
static bool isNewer(const String& remote, const String& local) {
    int rMaj, rMin, rPatch, lMaj, lMin, lPatch;
    sscanf(remote.c_str(), "%d.%d.%d", &rMaj, &rMin, &rPatch);
    sscanf(local.c_str(),  "%d.%d.%d", &lMaj, &lMin, &lPatch);

    if (rMaj != lMaj) return rMaj > lMaj;
    if (rMin != lMin) return rMin > lMin;
    return rPatch > lPatch;
}

// ── OTA update callback (progress to Serial) ─────────────────────────────────
static void onOTAProgress(int cur, int total) {
    Serial.printf("[OTA] Progress: %d / %d bytes (%.1f%%)\n",
                  cur, total, (total > 0) ? (cur * 100.0 / total) : 0);
}

// ── Main OTA check ────────────────────────────────────────────────────────────
void checkAndApplyOTA() {
    Serial.println("[OTA] Checking for firmware update...");
    Serial.printf("[OTA] Current version: %s\n", FIRMWARE_VERSION);

    // ── Step 1: Fetch version.txt ──────────────────────────────────────────
    WiFiClientSecure vClient;
    vClient.setInsecure();

    HTTPClient http;
    http.begin(vClient, GITHUB_VERSION_URL);
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] version.txt fetch failed (HTTP %d) — skipping OTA\n", code);
        http.end();
        return;
    }

    String remoteVersion = trim(http.getString());
    http.end();

    Serial.printf("[OTA] Remote version: %s\n", remoteVersion.c_str());

    // ── Step 2: Compare versions ───────────────────────────────────────────
    if (!isNewer(remoteVersion, String(FIRMWARE_VERSION))) {
        Serial.println("[OTA] Firmware is up to date.");
        return;
    }

    Serial.printf("[OTA] New firmware available (%s → %s). Starting update...\n",
                  FIRMWARE_VERSION, remoteVersion.c_str());

    // ── Step 3: Download and flash firmware.bin ────────────────────────────
    WiFiClientSecure fClient;
    fClient.setInsecure();   // Acceptable for this use case

    // Register progress callback
    httpUpdate.onProgress(onOTAProgress);

    // Reboot on success (default), don't reboot on failure
    httpUpdate.rebootOnUpdate(true);

    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    t_httpUpdate_return result = httpUpdate.update(fClient, FIRMWARE_BIN_URL);

    switch (result) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update FAILED: (%d) %s\n",
                          httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No update available (server agrees).");
            break;

        case HTTP_UPDATE_OK:
            // Normally unreachable — device reboots before this
            Serial.println("[OTA] Update OK — rebooting...");
            break;
    }
}
