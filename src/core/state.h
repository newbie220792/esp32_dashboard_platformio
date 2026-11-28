#pragma once
// Shared state store: the current value of everything the UI shows.
//
//  * Services write with state_set_*() / state_update_*() (any task).
//  * The UI reads copies with state_get_*() on the LVGL task. Every group has a
//    version that changes on each write, so a screen copies a group only when
//    state_version(group) differs from the one it last drew.
//  * Each write also posts a lightweight event (core/events.h) so the UI wakes
//    up at once; the UI's poll timer covers any event that got dropped.
//
// One FreeRTOS mutex guards all groups (writers are a few tasks, copies are
// small: HomeConfig ~7 KB is the largest). Never call LVGL from here.
#include <stdint.h>

#include "core/models.h"

enum class StateGroup : uint8_t { Net, WifiSetup, Pi5Stats, Services, Home, Weather, Settings, Location, Alerts, ApiHealth, Photos, Count };

struct NetState {
    bool     wifi_connected;   // STA associated *and* has an IP
    bool     mqtt_connected;
    int8_t   rssi;             // dBm, 0 when not connected
    uint32_t ip;               // IPv4, network byte order (0 = none)
};

struct Pi5StatsState {
    bool     have_data;
    uint32_t rx_ms;            // millis() of the last pi5/stats
    PiStats  stats;
};

struct WeatherState {
    bool        have_data;     // at least one good fetch
    bool        failing;       // the last fetch failed
    WeatherErr  err;           // why, when failing
    uint32_t    last_ok_ms;    // millis() of the last good fetch
    WeatherData data;
};

struct HomeState {
    bool       have_config;    // home/config received at least once
    HomeConfig cfg;            // switches carry on/pending, owned by home_service
};

struct LocationState {
    bool     from_api;         // false = NVS copy or the built-in default
    Location loc;
};

enum class AlertsOp : uint8_t { Idle, Loading, Clearing };
enum class AlertsErr : uint8_t { None, LoadFailed, ClearFailed };

struct AlertsState {
    // badge (GET /api/alerts/summary); kept as-is while offline
    bool         have_summary;
    AlertSummary summary;
    // list (GET /api/alerts), fetched while the Alerts screen is open
    bool         have_list;    // at least one good fetch
    AlertList    list;
    AlertsOp     op;           // request in flight
    AlertsErr    err;          // last list request outcome
};

struct ApiHealthState {
    ApiHealth status;
    uint16_t  rtt_ms;          // Ok only
};

// Photos tab (task 10), written by photo_service. The two RGB565 frames live in
// PSRAM and never move once allocated; `front` is the one to show. While the
// UI fades to a new front, the service leaves the other buffer alone.
struct PhotoState {
    PhotoStatus status;
    bool        have_frame;   // frames[front] holds a decoded photo
    bool        paused;       // Pause/Play
    uint8_t     dwell_s;      // 5 or 10 (NVS photo_s)
    uint8_t     front;        // 0 / 1
    uint16_t    index;        // shown photo, 0-based, in the current list
    uint16_t    count;        // photos in the list
    uint32_t    frame_seq;    // +1 each time a new frame becomes `front`
    uint16_t   *frames[2];    // PHOTO_W x PHOTO_H RGB565 (LVGL byte order), nullptr until the tab opens
};

void state_init();

// Zeroed buffer in PSRAM (falls back to internal RAM). For the ~7 KB HomeConfig /
// HomeState copies, so they don't eat the internal RAM TLS needs. Never freed.
void *state_alloc_large(size_t bytes);
uint32_t state_version(StateGroup g);   // cheap, any task

// ---- net (wifi_service, mqtt_service) ----
NetState state_get_net();
void state_set_wifi(bool connected, int8_t rssi, uint32_t ip);
void state_set_mqtt(bool connected);

// ---- Wi-Fi setup flow (wifi_service) ----
WifiSetupInfo state_get_wifi_setup();
void state_set_wifi_setup(const WifiSetupInfo &info);   // bumps the version even if unchanged

// ---- pi5 (pi5_service) ----
void state_get_pi5_stats(Pi5StatsState &out);
void state_set_pi5_stats(const PiStats &s);
void state_get_services(ServiceList &out);
void state_set_services(const ServiceList &list);

// ---- weather (weather_service) ----
void state_get_weather(WeatherState &out);
void state_set_weather(const WeatherData &d);
void state_set_weather_error(WeatherErr e);

// ---- home (home_service) ----
void state_get_home(HomeState &out);
// Runs `fn(cfg, ctx)` under the lock and bumps the version if it returns true.
// Lets home_service edit switches in place without copying 7 KB twice.
void state_update_home(bool (*fn)(HomeState &st, void *ctx), void *ctx);

// ---- settings (settings_service) ----
Settings state_get_settings();
void state_set_settings(const Settings &s);

// ---- location (location_service) ----
LocationState state_get_location();
void state_set_location(const Location &loc, bool from_api);   // bumps only on change

// ---- alerts (alerts_service) ----
void state_get_alerts(AlertsState &out);
void state_set_alerts_summary(const AlertSummary &s);           // bumps only on change
void state_set_alerts_op(AlertsOp op);
void state_set_alerts_list(const AlertList &list);              // op Idle, err None
void state_set_alerts_error(AlertsErr err);                     // op Idle, list kept
void state_alerts_cleared();                                    // empty list, unacked 0

// ---- photos (photo_service) ----
PhotoState state_get_photos();
// Runs `fn(st, ctx)` under the lock and bumps the version if it returns true.
void state_update_photos(bool (*fn)(PhotoState &st, void *ctx), void *ctx);

// ---- Pi 5 health check (sysinfo_service, task 9) ----
ApiHealthState state_get_api_health();
void state_set_api_health(ApiHealth status, uint16_t rtt_ms);   // bumps only on change
