#pragma once
// Plain data models shared by parsers, network tasks and UI.
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
    // UI-side bookkeeping (not from JSON)
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
