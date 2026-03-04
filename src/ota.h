#pragma once
#include <Arduino.h>

// Check GitHub for a newer firmware version.
// If found, downloads firmware.bin and flashes it.
// Device reboots automatically if update succeeds.
// Safe to call at any time — no-op if already up to date or if fetch fails.
void checkAndApplyOTA();
