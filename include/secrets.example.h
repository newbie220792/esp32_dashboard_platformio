#pragma once
// Copy this file to include/secrets.h (gitignored) and fill in your values.

#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"

#define MQTT_HOST      "192.168.1.50"   // Pi 5 / broker IP or hostname
#define MQTT_PORT      1883
#define MQTT_USER      ""               // empty = anonymous
#define MQTT_PASSWORD  ""

// Password of the "ESP32-Setup" configuration access point (8-63 chars).
// Shown on the setup screen and in its QR code. "" = open network.
#define SETUP_AP_PASSWORD  "change-me-123"
