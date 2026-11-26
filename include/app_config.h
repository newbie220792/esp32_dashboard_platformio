#pragma once
// Non-secret application settings. Credentials live in secrets.h (read only by
// core/config.cpp); board GPIOs live in board_pins.h.

// ---------------- Navigation ----------------
#define AUTO_ROTATE_MS     0       // switch tile automatically (ms); 0 = off

// ---------------- Wi-Fi setup (WiFiManager portal) ----------------
#define WIFI_CONNECT_TIMEOUT_MS 18000   // per network, secrets.h / NVS
#define WIFI_FAIL_COUNTDOWN_MS  5000    // screen 4 -> portal
#define SETUP_AP_SSID           "ESP32-Setup"
#define SETUP_AP_IP             "192.168.4.1"
#define WIFI_RESET_HOLD_MS      5000    // hold BOOT: erase saved Wi-Fi, open portal

// ---------------- MQTT ----------------
#define MQTT_TOPIC_STATS       "pi5/stats"
#define MQTT_TOPIC_SERVICES    "pi5/services"
#define MQTT_TOPIC_HOME_CONFIG "home/config"
#define MQTT_TOPIC_HOME_STATE  "home/+/+/+/state"   // .../set is published
#define SWITCH_PENDING_MS      3000   // revert dimmed switch if no .../state
#define MQTT_STALE_MS      10000   // no message for this long -> dot gray


// ---------------- Weather (Open-Meteo) ----------------
#define WEATHER_LOCATION   "Ho Chi Minh"
#define WEATHER_LAT        10.8231
#define WEATHER_LON        106.6297
#define WEATHER_REFRESH_MS (10UL * 60UL * 1000UL)
#define WEATHER_RETRY_MS     (30UL * 1000UL)         // first retry; doubles on each failure
#define WEATHER_RETRY_MAX_MS (10UL * 60UL * 1000UL)   // backoff cap

// ---------------- Time ----------------
#define NTP_SERVER_1  "pool.ntp.org"
#define NTP_SERVER_2  "time.google.com"
#define TZ_POSIX      "<+07>-7"    // Asia/Ho_Chi_Minh, no DST
