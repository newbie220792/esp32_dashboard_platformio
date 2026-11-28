#pragma once
// Credentials and persisted settings. The only file that includes secrets.h;
// everything else asks here. No network logic.
#include <stddef.h>
#include <stdint.h>

#include "core/models.h"

// secrets.h (compile-time)
const char *config_wifi_ssid();
const char *config_wifi_password();
const char *config_mqtt_host();
uint16_t config_mqtt_port();
const char *config_mqtt_user();       // "" = anonymous
const char *config_mqtt_password();
const char *config_setup_ap_password();   // "" = open "ESP32-Setup" AP

// Pi 5 REST API (alerts + location, task 8). Plain HTTP inside the LAN.
const char *config_pi5_api_host();    // "" = not configured: alerts/location stay offline
uint16_t config_pi5_api_port();
const char *config_pi5_api_token();   // sent as "Authorization: Bearer <token>"

// Network entered through the setup portal (NVS namespace "wifi_cfg").
bool config_load_saved_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
void config_save_wifi(const char *ssid, const char *pass);
void config_erase_saved_wifi();

// Settings screen values (NVS namespace "settings": bright, sleep_s, unit).
// Missing keys get the defaults (70 %, 30 s, deg C); values are clamped.
Settings config_load_settings();
void config_save_settings(const Settings &s);

// Last location received from the Pi 5 (NVS namespace "location").
bool config_load_location(Location &out);
void config_save_location(const Location &loc);
