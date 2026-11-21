#pragma once
// Non-secret application settings. Credentials live in secrets.h.

// ---------------- Display: ILI9341 over SPI (ESP32-S3) ----------------
// Avoid GPIO 26-32 (flash) and 35-37 (octal PSRAM on R8 modules).
#define TFT_SPI_HOST   SPI2_HOST
#define TFT_PIN_SCLK   12
#define TFT_PIN_MOSI   11
#define TFT_PIN_MISO   13   // -1 if not wired
#define TFT_PIN_CS     10
#define TFT_PIN_DC     9
#define TFT_PIN_RST    14   // -1 if tied to EN
#define TFT_PIN_BL     21   // -1 if backlight is hard-wired
#define TFT_SPI_FREQ   40000000
#define TFT_ROTATION   1    // 1 or 3 = landscape 320x240
#define TFT_INVERT     false
#define TFT_RGB_ORDER  false

#define SCREEN_W 320
#define SCREEN_H 240

// ---------------- Navigation (no touch panel) ----------------
#define NAV_BUTTON_PIN     0       // BOOT button, active low; -1 to disable
#define AUTO_ROTATE_MS     15000   // switch tile automatically; 0 = off

// ---------------- MQTT ----------------
#define MQTT_TOPIC_STATS   "pi5/stats"
#define MQTT_STALE_MS      10000   // no message for this long -> dot gray

// ---------------- Weather (Open-Meteo) ----------------
#define WEATHER_LOCATION   "Ho Chi Minh"
#define WEATHER_LAT        10.8231
#define WEATHER_LON        106.6297
#define WEATHER_REFRESH_MS (10UL * 60UL * 1000UL)
#define WEATHER_RETRY_MS   (30UL * 1000UL)

// ---------------- Time ----------------
#define NTP_SERVER_1  "pool.ntp.org"
#define NTP_SERVER_2  "time.google.com"
#define TZ_POSIX      "<+07>-7"    // Asia/Ho_Chi_Minh, no DST
