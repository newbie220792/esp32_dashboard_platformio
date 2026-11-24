#pragma once
// Non-secret application settings. Credentials live in secrets.h.

// ---------------- Display: Guition JC4827W543C, NV3041A over QSPI ----------------
// Pins from the board's ESPHome config (devices.esphome.io/devices/guition-jc4827543c).
// GPIO 35-37 are taken by the octal PSRAM of the N4R8 module.
#define TFT_PIN_CS     45
#define TFT_PIN_SCK    47
#define TFT_PIN_D0     21
#define TFT_PIN_D1     48
#define TFT_PIN_D2     40
#define TFT_PIN_D3     39
#define TFT_PIN_BL     1      // backlight, driven fully on (no dimming)
#define TFT_ROTATION   0      // 0 or 2 (180 deg); the NV3041A cannot do 90/270
#define TFT_IPS        true   // IPS panel: colour inversion on

#define SCREEN_W 480
#define SCREEN_H 272

// ---------------- Touch: GT911 via TouchLib (I2C) ----------------
#define TOUCH_ENABLED   1
#define TOUCH_PIN_SDA   8
#define TOUCH_PIN_SCL   4
#define TOUCH_PIN_RST   38      // -1 if not wired
#define TOUCH_PIN_INT   3       // unused (polled), kept for reference
#define TOUCH_I2C_ADDR  0x5D    // GT911_SLAVE_ADDRESS1 (0x14 = ADDRESS2)
// Raw coordinate range reported by the controller, and how it maps onto the
// 480x272 landscape screen (if TFT_ROTATION is 2, set both TOUCH_INVERT_* to 1).
// Set TOUCH_DEBUG 1 to print raw points on Serial, then tap the corners and adjust.
#define TOUCH_RAW_W     480
#define TOUCH_RAW_H     272
#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0
#define TOUCH_DEBUG     0

// ---------------- Navigation ----------------
#define NAV_BUTTON_PIN     0       // BOOT button, active low; -1 to disable
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
