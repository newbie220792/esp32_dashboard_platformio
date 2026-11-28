#include "core/state.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <string.h>

#include "core/events.h"

static SemaphoreHandle_t s_lock;
static volatile uint32_t s_version[(int)StateGroup::Count];

static NetState s_net;
static WifiSetupInfo s_wifi_setup;
static Pi5StatsState s_pi5;
static ServiceList s_services;
static WeatherState s_weather;
static HomeState *s_home_p;   // PSRAM, see state_init()
static Settings s_settings;
static LocationState s_location;
static AlertsState s_alerts;
static ApiHealthState s_api_health;

namespace {
struct Lock {
    Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_lock); }
};
}   // namespace

// Call with the lock held.
static void bump(StateGroup g) { s_version[(int)g]++; }

// Call after releasing the lock: wakes the UI (best effort, never blocks).
static void notify(StateGroup g) { events_post_state(g); }

void *state_alloc_large(size_t bytes) {
    void *p = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_calloc(1, bytes, MALLOC_CAP_8BIT);
}

void state_init() {
    s_lock = xSemaphoreCreateMutex();
    s_home_p = static_cast<HomeState *>(state_alloc_large(sizeof(HomeState)));
    s_wifi_setup.state = WifiSetupState::Connecting;
}

uint32_t state_version(StateGroup g) { return s_version[(int)g]; }

// ---------------- net ----------------

NetState state_get_net() {
    Lock l;
    return s_net;
}

void state_set_wifi(bool connected, int8_t rssi, uint32_t ip) {
    bool changed;
    {
        Lock l;
        const NetState before = s_net;
        s_net.wifi_connected = connected;
        s_net.rssi = connected ? rssi : 0;
        s_net.ip = connected ? ip : 0;
        changed = before.wifi_connected != s_net.wifi_connected || before.ip != s_net.ip ||
                  before.rssi != s_net.rssi;
        if (changed) bump(StateGroup::Net);
    }
    if (changed) notify(StateGroup::Net);
}

void state_set_mqtt(bool connected) {
    bool changed;
    {
        Lock l;
        changed = s_net.mqtt_connected != connected;
        s_net.mqtt_connected = connected;
        if (changed) bump(StateGroup::Net);
    }
    if (changed) notify(StateGroup::Net);
}

// ---------------- Wi-Fi setup ----------------

WifiSetupInfo state_get_wifi_setup() {
    Lock l;
    return s_wifi_setup;
}

void state_set_wifi_setup(const WifiSetupInfo &info) {
    {
        Lock l;
        s_wifi_setup = info;
        bump(StateGroup::WifiSetup);   // e.g. FailedSaved twice in a row restarts the countdown
    }
    notify(StateGroup::WifiSetup);
}

// ---------------- pi5 ----------------

void state_get_pi5_stats(Pi5StatsState &out) {
    Lock l;
    out = s_pi5;
}

void state_set_pi5_stats(const PiStats &st) {
    {
        Lock l;
        s_pi5.stats = st;
        s_pi5.have_data = true;
        s_pi5.rx_ms = millis();
        bump(StateGroup::Pi5Stats);
    }
    notify(StateGroup::Pi5Stats);
}

void state_get_services(ServiceList &out) {
    Lock l;
    out = s_services;
}

void state_set_services(const ServiceList &list) {
    {
        Lock l;
        s_services = list;
        bump(StateGroup::Services);
    }
    notify(StateGroup::Services);
}

// ---------------- weather ----------------

void state_get_weather(WeatherState &out) {
    Lock l;
    out = s_weather;
}

void state_set_weather(const WeatherData &d) {
    {
        Lock l;
        s_weather.data = d;
        s_weather.have_data = true;
        s_weather.failing = false;
        s_weather.last_ok_ms = millis();
        bump(StateGroup::Weather);
    }
    notify(StateGroup::Weather);
}

void state_set_weather_error(WeatherErr e) {
    {
        Lock l;
        s_weather.failing = true;
        s_weather.err = e;
        bump(StateGroup::Weather);
    }
    notify(StateGroup::Weather);
}

// ---------------- home ----------------

void state_get_home(HomeState &out) {
    Lock l;
    out = *s_home_p;
}

void state_update_home(bool (*fn)(HomeState &st, void *ctx), void *ctx) {
    bool changed;
    {
        Lock l;
        changed = fn(*s_home_p, ctx);
        if (changed) bump(StateGroup::Home);
    }
    if (changed) notify(StateGroup::Home);
}

// ---------------- settings ----------------

Settings state_get_settings() {
    Lock l;
    return s_settings;
}

void state_set_settings(const Settings &s) {
    bool changed;
    {
        Lock l;
        changed = s_settings.brightness != s.brightness || s_settings.sleep_s != s.sleep_s ||
                  s_settings.unit != s.unit;
        s_settings = s;
        if (changed) bump(StateGroup::Settings);
    }
    if (changed) notify(StateGroup::Settings);
}

// ---------------- location ----------------

LocationState state_get_location() {
    Lock l;
    return s_location;
}

void state_set_location(const Location &loc, bool from_api) {
    bool changed;
    {
        Lock l;
        changed = strcmp(s_location.loc.city, loc.city) || s_location.loc.lat != loc.lat ||
                  s_location.loc.lon != loc.lon || s_location.from_api != from_api;
        s_location.loc = loc;
        s_location.from_api = from_api;
        if (changed) bump(StateGroup::Location);
    }
    if (changed) notify(StateGroup::Location);
}

// ---------------- alerts ----------------

void state_get_alerts(AlertsState &out) {
    Lock l;
    out = s_alerts;
}

void state_set_alerts_summary(const AlertSummary &s) {
    bool changed;
    {
        Lock l;
        changed = !s_alerts.have_summary || s_alerts.summary.unacked != s.unacked ||
                  s_alerts.summary.critical != s.critical || s_alerts.summary.latest_id != s.latest_id;
        s_alerts.summary = s;
        s_alerts.have_summary = true;
        if (changed) bump(StateGroup::Alerts);
    }
    if (changed) notify(StateGroup::Alerts);
}

void state_set_alerts_op(AlertsOp op) {
    {
        Lock l;
        s_alerts.op = op;
        bump(StateGroup::Alerts);
    }
    notify(StateGroup::Alerts);
}

void state_set_alerts_list(const AlertList &list) {
    {
        Lock l;
        s_alerts.list = list;
        s_alerts.have_list = true;
        s_alerts.op = AlertsOp::Idle;
        s_alerts.err = AlertsErr::None;
        bump(StateGroup::Alerts);
    }
    notify(StateGroup::Alerts);
}

void state_set_alerts_error(AlertsErr err) {
    {
        Lock l;
        s_alerts.op = AlertsOp::Idle;
        s_alerts.err = err;
        bump(StateGroup::Alerts);
    }
    notify(StateGroup::Alerts);
}

void state_alerts_cleared() {
    {
        Lock l;
        s_alerts.list.count = 0;
        s_alerts.have_list = true;
        s_alerts.summary.unacked = 0;
        s_alerts.summary.critical = 0;
        s_alerts.have_summary = true;
        s_alerts.op = AlertsOp::Idle;
        s_alerts.err = AlertsErr::None;
        bump(StateGroup::Alerts);
    }
    notify(StateGroup::Alerts);
}

// ---------------- Pi 5 health check ----------------

ApiHealthState state_get_api_health() {
    Lock l;
    return s_api_health;
}

void state_set_api_health(ApiHealth status, uint16_t rtt_ms) {
    bool changed;
    {
        Lock l;
        if (status != ApiHealth::Ok) rtt_ms = 0;
        changed = s_api_health.status != status || s_api_health.rtt_ms != rtt_ms;
        s_api_health.status = status;
        s_api_health.rtt_ms = rtt_ms;
        if (changed) bump(StateGroup::ApiHealth);
    }
    if (changed) notify(StateGroup::ApiHealth);
}
