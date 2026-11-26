#include "core/state.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

#include "core/events.h"

static SemaphoreHandle_t s_lock;
static volatile uint32_t s_version[(int)StateGroup::Count];

static NetState s_net;
static WifiSetupInfo s_wifi_setup;
static Pi5StatsState s_pi5;
static ServiceList s_services;
static WeatherState s_weather;
static HomeState *s_home_p;   // PSRAM, see state_init()

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
