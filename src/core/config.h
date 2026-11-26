#pragma once
// Credentials and persisted settings. The only file that includes secrets.h;
// everything else asks here. No network logic.
#include <stddef.h>
#include <stdint.h>

// secrets.h (compile-time)
const char *config_wifi_ssid();
const char *config_wifi_password();
const char *config_mqtt_host();
uint16_t config_mqtt_port();
const char *config_mqtt_user();       // "" = anonymous
const char *config_mqtt_password();
const char *config_setup_ap_password();   // "" = open "ESP32-Setup" AP

// Network entered through the setup portal (NVS namespace "wifi_cfg").
bool config_load_saved_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
void config_save_wifi(const char *ssid, const char *pass);
void config_erase_saved_wifi();
