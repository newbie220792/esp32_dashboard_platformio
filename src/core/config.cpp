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

// Older secrets.h files have no Pi 5 API block: build anyway, API stays off.
#ifndef PI5_API_HOST
#define PI5_API_HOST ""   // see secrets.example.h
#endif
#ifndef PI5_API_PORT
#define PI5_API_PORT 8080
#endif
#ifndef PI5_API_TOKEN
#define PI5_API_TOKEN ""
#endif

#define NVS_NAMESPACE "wifi_cfg"
#define NVS_SETTINGS  "settings"
#define NVS_LOCATION  "location"

const char *config_wifi_ssid() { return WIFI_SSID; }
const char *config_wifi_password() { return WIFI_PASSWORD; }
const char *config_mqtt_host() { return MQTT_HOST; }
uint16_t config_mqtt_port() { return MQTT_PORT; }
const char *config_mqtt_user() { return MQTT_USER; }
const char *config_mqtt_password() { return MQTT_PASSWORD; }
const char *config_setup_ap_password() { return SETUP_AP_PASSWORD; }
const char *config_pi5_api_host() { return PI5_API_HOST; }
uint16_t config_pi5_api_port() { return PI5_API_PORT; }
const char *config_pi5_api_token() { return PI5_API_TOKEN; }

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

// ---------------- settings ----------------

static uint16_t valid_sleep(uint16_t s) {
    return (s == 0 || s == 15 || s == 30 || s == 60) ? s : 30;
}

Settings config_load_settings() {
    Settings s{BRIGHTNESS_DEFAULT, 30, TempUnit::C};
    Preferences p;
    if (p.begin(NVS_SETTINGS, true)) {   // absent namespace = defaults
        s.brightness = p.getUChar("bright", BRIGHTNESS_DEFAULT);
        s.sleep_s = p.getUShort("sleep_s", 30);
        s.unit = p.getUChar("unit", 0) == 1 ? TempUnit::F : TempUnit::C;
        p.end();
    }
    if (s.brightness < BRIGHTNESS_MIN) s.brightness = BRIGHTNESS_MIN;
    if (s.brightness > 100) s.brightness = 100;
    s.sleep_s = valid_sleep(s.sleep_s);
    return s;
}

void config_save_settings(const Settings &s) {
    Preferences p;
    if (!p.begin(NVS_SETTINGS, false)) return;
    // put* rewrites flash only when the value differs, so saving all three is cheap
    p.putUChar("bright", s.brightness);
    p.putUShort("sleep_s", s.sleep_s);
    p.putUChar("unit", s.unit == TempUnit::F ? 1 : 0);
    p.end();
}

// ---------------- photos ----------------

uint8_t config_load_photo_s() {
    Preferences p;
    uint8_t s = 10;
    if (p.begin(NVS_SETTINGS, true)) {
        s = p.getUChar("photo_s", 10);
        p.end();
    }
    return s == 5 ? 5 : 10;
}

void config_save_photo_s(uint8_t seconds) {
    Preferences p;
    if (!p.begin(NVS_SETTINGS, false)) return;
    p.putUChar("photo_s", seconds == 5 ? 5 : 10);
    p.end();
}

// ---------------- location ----------------

bool config_load_location(Location &out) {
    Preferences p;
    if (!p.begin(NVS_LOCATION, true)) return false;
    String city = p.getString("city", "");
    const float lat = p.getFloat("lat", NAN), lon = p.getFloat("lon", NAN);
    p.end();
    if (city.isEmpty() || isnan(lat) || isnan(lon)) return false;
    copy(out.city, sizeof out.city, city.c_str());
    out.lat = lat;
    out.lon = lon;
    return true;
}

void config_save_location(const Location &loc) {
    Preferences p;
    if (!p.begin(NVS_LOCATION, false)) return;
    p.putString("city", loc.city);
    p.putFloat("lat", loc.lat);
    p.putFloat("lon", loc.lon);
    p.end();
}
