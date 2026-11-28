#pragma once
// Plain data models shared by parsers, services, the state store and UI.
// No Arduino / FreeRTOS / LVGL includes: also compiled by the native tests.

#include <stddef.h>
#include <stdint.h>

// ---------------- pi5/stats ----------------
struct PiStats {
    float cpu;        // %
    float ram;        // %
    float ram_used;   // GB
    float ram_total;  // GB
    float temp;       // deg C
};

// ---------------- pi5/services ----------------
#define MAX_SERVICES     10
#define SERVICE_NAME_LEN 24

struct ServiceInfo {
    char    name[SERVICE_NAME_LEN];
    bool    running;     // anything other than "running" counts as stopped
    int32_t port;        // -1 = none (JSON null / missing)
};

struct ServiceList {
    uint8_t     count;
    ServiceInfo items[MAX_SERVICES];
};

// ---------------- home/config ----------------
#define HOME_MAX_ROOMS       8
#define HOME_MAX_PANELS      6     // per room
#define HOME_SWITCHES        3     // exactly 3 per panel
#define HOME_ID_LEN          16
#define HOME_NAME_LEN        20

enum class SwitchIcon : uint8_t { Bulb, Fan, Plug, Snow };

struct HomeSwitch {
    bool       present;            // false = placeholder (panel had < 3)
    uint16_t   id;                 // number used in the MQTT topic
    char       name[HOME_NAME_LEN];
    SwitchIcon icon;
    bool       on;
    // home_service bookkeeping (not from JSON): a .../set is in flight
    bool       pending;
    uint32_t   pending_since_ms;
};

struct HomePanel {
    char       id[HOME_ID_LEN];
    char       name[HOME_NAME_LEN];
    HomeSwitch switches[HOME_SWITCHES];
};

struct HomeRoom {
    char      id[HOME_ID_LEN];
    char      name[HOME_NAME_LEN];
    uint8_t   panel_count;
    HomePanel panels[HOME_MAX_PANELS];
};

struct HomeConfig {
    uint8_t  room_count;
    HomeRoom rooms[HOME_MAX_ROOMS];
};

// home/<room>/<panel>/<id>/state
struct SwitchState {
    char     room[HOME_ID_LEN];
    char     panel[HOME_ID_LEN];
    uint16_t id;
    bool     on;
};

// ---------------- weather (Open-Meteo) ----------------
#define WEATHER_HOURS 4

struct WeatherHour {
    uint8_t hour;     // local hour 0..23
    int16_t code;     // WMO weather code
    bool    is_day;
    float   temp;
};

struct WeatherData {
    float   temp;
    float   feels_like;
    float   humidity;
    float   wind_kmh;
    int16_t rain_chance;   // % for the hour in progress, -1 = unknown
    int16_t code;
    bool    is_day;
    WeatherHour hours[WEATHER_HOURS];
    uint8_t hour_count;
};

// Why the last weather fetch failed (shown on the weather card).
enum class WeatherErr : uint8_t { NoNetwork, Timeout, ServerError, BadData };

// ---------------- settings (NVS, task 8) ----------------
enum class TempUnit : uint8_t { C, F };

#define BRIGHTNESS_MIN     10    // %, the screen never goes fully dark from the slider
#define BRIGHTNESS_DEFAULT 70

struct Settings {
    uint8_t  brightness;   // BRIGHTNESS_MIN..100 %
    uint16_t sleep_s;      // screen off after this much inactivity, 0 = never
    TempUnit unit;
};

// Data always stays in deg C; only the labels convert.
inline float temp_in_unit(float c, TempUnit u) { return u == TempUnit::F ? c * 9.0f / 5.0f + 32.0f : c; }

// ---------------- Pi 5 location (GET /api/location) ----------------
#define CITY_LEN 32

struct Location {
    char  city[CITY_LEN];   // ASCII only (see ascii_sanitize)
    float lat;
    float lon;
};

// ---------------- Pi 5 alerts (GET /api/alerts, /api/alerts/summary) ----------------
#define ALERTS_MAX        10
#define ALERT_TITLE_LEN   48   // Pi 5 cuts titles at 40 chars, details at 60
#define ALERT_DETAIL_LEN  72

enum class AlertLevel : uint8_t { Critical, Warning, Info };

struct Alert {
    uint32_t   id;
    AlertLevel level;
    char       title[ALERT_TITLE_LEN];
    char       detail[ALERT_DETAIL_LEN];
    uint32_t   ts;      // epoch seconds (UTC)
    uint16_t   count;   // repeats merged by the Pi 5
};

struct AlertList {
    uint8_t count;
    Alert   items[ALERTS_MAX];   // newest first
};

struct AlertSummary {
    uint16_t unacked;
    uint16_t critical;
    uint32_t latest_id;
};

// ---------------- System info screen (task 9) ----------------
// Pi 5 reachability: GET /api/health (or /api/alerts/summary) while the screen is open.
enum class ApiHealth : uint8_t {
    Unknown,         // not tried yet / no Wi-Fi: gray "--"
    Ok,              // 200: green, round trip in ms
    Error,           // other status, error or timeout: red
    NotConfigured,   // PI5_API_HOST missing in secrets.h
};

// Snapshot for the System info screen. Fixed-size strings, no allocation.
struct SysInfo {
    // network
    bool      wifi_connected;
    char      ssid[33];       // ASCII only (ascii_sanitize), "" when not connected
    uint32_t  ip;             // IPv4, network byte order, 0 = none
    int8_t    rssi;           // dBm, 0 = no reading
    bool      mqtt_connected;
    ApiHealth api;
    uint16_t  api_rtt_ms;
    bool      time_synced;
    uint32_t  last_sync;      // epoch s of the last SNTP sync, 0 = none yet
    // device
    char      fw_version[16];
    uint64_t  uptime_s;
    uint32_t  heap_free, heap_total;
    uint32_t  psram_free, psram_total;
    uint32_t  sketch_size;    // 0 = not measured yet
    uint32_t  app_size;       // running app partition (huge_app.csv: ~3 MB, no OTA)
};

// ---------------- Photos tab (task 10) ----------------
// GET /api/photos: the Pi 5 decides which photos and in which order.
#define PHOTO_W         480
#define PHOTO_H         272
#define PHOTO_MAX_ITEMS 100
#define PHOTO_ID_LEN    13      // 12 ASCII chars + NUL
#define PHOTO_REV_LEN   17
#define PHOTO_MAX_BYTES (96 * 1024)   // JPEG download buffer; the Pi 5 keeps photos <= 90 KB

struct PhotoList {
    char     rev[PHOTO_REV_LEN];
    uint16_t count;
    char     ids[PHOTO_MAX_ITEMS][PHOTO_ID_LEN];
};

enum class PhotoStatus : uint8_t {
    Idle,            // tab never opened
    Loading,         // first photo on its way (spinner)
    Showing,         // a photo is on screen
    Empty,           // the Pi 5 list is empty: "No photos"
    Offline,         // no Wi-Fi / Pi 5 errors: last photo stays, "Offline" chip
    NoMemory,        // PSRAM buffers could not be allocated
    NotConfigured,   // PI5_API_HOST missing in secrets.h
};

// ---------------- Wi-Fi setup flow (wifi_setup.cpp -> ui_wifi.cpp) ----------------
enum class WifiSetupState : uint8_t {
    Connecting,       // trying secrets.h / saved network (screen: spinner)
    FailedSaved,      // screen 4: countdown, Retry / Setup now
    Portal,           // screen 5: AP + captive portal open, QR
    ConnectingNew,    // credentials submitted in the portal, trying them
    FailedNew,        // screen 6: portal still open, Attempt N
    Connected,        // main UI
};

enum class WifiFailReason : uint8_t { None, NotFound, WrongPassword, ConnectFailed, Timeout };

struct WifiSetupInfo {
    WifiSetupState state;
    WifiFailReason reason;
    uint16_t       attempt;    // portal attempts so far (FailedNew / ConnectingNew)
    char           ssid[33];   // network being tried / that failed
};
