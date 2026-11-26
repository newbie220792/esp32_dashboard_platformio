#include "core/config.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "secrets.h"

#ifndef SETUP_AP_PASSWORD
#define SETUP_AP_PASSWORD ""   // see secrets.example.h
#endif
static_assert(sizeof(SETUP_AP_PASSWORD) == 1 || sizeof(SETUP_AP_PASSWORD) >= 9,
              "SETUP_AP_PASSWORD must be empty or at least 8 characters");

#define NVS_NAMESPACE "wifi_cfg"

const char *config_wifi_ssid() { return WIFI_SSID; }
const char *config_wifi_password() { return WIFI_PASSWORD; }
const char *config_mqtt_host() { return MQTT_HOST; }
uint16_t config_mqtt_port() { return MQTT_PORT; }
const char *config_mqtt_user() { return MQTT_USER; }
const char *config_mqtt_password() { return MQTT_PASSWORD; }
const char *config_setup_ap_password() { return SETUP_AP_PASSWORD; }

static void copy(char *dst, size_t n, const char *src) {
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

bool config_load_saved_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return false;   // namespace absent = nothing saved
    String s = p.getString("ssid", "");
    String pw = p.getString("pass", "");
    p.end();
    if (s.isEmpty()) return false;
    copy(ssid, ssid_len, s.c_str());
    copy(pass, pass_len, pw.c_str());
    return true;
}

void config_save_wifi(const char *ssid, const char *pass) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
    Serial.printf("[config] saved '%s' to NVS\n", ssid);
}

void config_erase_saved_wifi() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
    Serial.println("[config] saved network erased");
}
