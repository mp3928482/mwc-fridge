#pragma once
#include <Arduino.h>

#define ONE_WIRE_BUS 4   // GPIO pin for DS18B20 data line

// Initialize the DS18B20 sensor. Call once in setup().
void initSensor();

// Read temperature, POST to Google Sheet, check alert thresholds.
// Handles WiFi check, sensor read, HTTP POST, and out-of-range alerting.
void readAndLog();
