#pragma once
// Non-secret application settings. Credentials live in secrets.h (read only by
// core/config.cpp); board GPIOs live in board_pins.h.

// ---------------- Firmware version ----------------
// Set in platformio.ini (build_flags = -D FW_VERSION=\"x.y.z\"), the one source
// for the System info screen and the OTA check.
#ifndef FW_VERSION
#warning "FW_VERSION is not set in platformio.ini: System info shows v?"
#define FW_VERSION "?"
#endif

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


// ---------------- Location (GET /api/location on the Pi 5) ----------------
// Used until the Pi 5 answers once (then its last answer is kept in NVS).
// The coordinates are the Pi 5's job; these are only a fallback.
// TODO(task 8): confirm the point to use for TP Dong Nai (now: Bien Hoa centre).
#define DEFAULT_CITY        "TP Dong Nai"
#define DEFAULT_LAT         10.9574
#define DEFAULT_LON         106.8427
#define LOCATION_REFRESH_MS (5UL * 60UL * 1000UL)   // notice a change made on the Pi 5
#define LOCATION_RETRY_MS   (30UL * 1000UL)

// ---------------- Alerts (Pi 5 REST API) ----------------
#define PI5_API_TIMEOUT_MS      5000
#define ALERTS_SUMMARY_MS       (20UL * 1000UL)        // bell badge refresh
#define ALERTS_SUMMARY_RETRY_MS (30UL * 1000UL)        // then 60 s, 120 s, ... capped below
#define ALERTS_SUMMARY_MAX_MS   (5UL * 60UL * 1000UL)
#define ALERTS_LIST_MS          (20UL * 1000UL)        // list refresh while the screen is open

// ---------------- System info screen (task 9) ----------------
#define SYSINFO_REFRESH_MS      1000    // lv_timer while the screen is shown
#define SYSINFO_API_MS          (10UL * 1000UL)   // Pi 5 health check while the screen is shown
#define SYSINFO_API_TIMEOUT_MS  3000
#define SCREEN_ANIM_MS          200     // Settings <-> System info slide

// ---------------- Screen off (Settings) ----------------
#define SCREEN_DIM_LEAD_MS 5000   // dim this long before the backlight turns off
#define SCREEN_DIM_PCT     10     // dimmed brightness, % of the normal level (min 5 %)

// ---------------- Weather (Open-Meteo) ----------------
#define WEATHER_REFRESH_MS (10UL * 60UL * 1000UL)
#define WEATHER_RETRY_MS     (30UL * 1000UL)         // first retry; doubles on each failure
#define WEATHER_RETRY_MAX_MS (10UL * 60UL * 1000UL)   // backoff cap

// ---------------- Time ----------------
#define NTP_SERVER_1  "pool.ntp.org"
#define NTP_SERVER_2  "time.google.com"
#define TZ_POSIX      "<+07>-7"    // Asia/Ho_Chi_Minh, no DST
