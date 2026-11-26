#include "services/home_service.h"

#include <Arduino.h>
#include <string.h>

#include "app_config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/mqtt_service.h"

#define TICK_MS 200

// ---------------- helpers (run under the state lock) ----------------

static HomeSwitch *find_switch(HomeConfig &cfg, const char *room, const char *panel, uint16_t id) {
    for (int r = 0; r < cfg.room_count; r++) {
        HomeRoom &rm = cfg.rooms[r];
        if (strcmp(rm.id, room)) continue;
        for (int p = 0; p < rm.panel_count; p++) {
            HomePanel &pn = rm.panels[p];
            if (strcmp(pn.id, panel)) continue;
            for (HomeSwitch &sw : pn.switches)
                if (sw.present && sw.id == id) return &sw;
        }
    }
    return nullptr;
}

// Calls fn on every switch; returns true if any call returned true.
template <typename F>
static bool for_each_switch(HomeConfig &cfg, F fn) {
    bool any = false;
    for (int r = 0; r < cfg.room_count; r++)
        for (int p = 0; p < cfg.rooms[r].panel_count; p++)
            for (HomeSwitch &sw : cfg.rooms[r].panels[p].switches) any |= fn(sw);
    return any;
}

// ---------------- MQTT handlers (mqtt task) ----------------

static void on_config(const char *, const uint8_t *payload, unsigned len) {
    // mqtt task only; ~7 KB, so PSRAM instead of the stack.
    static HomeConfig *cfg_p = static_cast<HomeConfig *>(state_alloc_large(sizeof(HomeConfig)));
    HomeConfig &cfg = *cfg_p;
    if (!parse_home_config((const char *)payload, len, cfg)) {
        Serial.printf("[home] bad home/config JSON (%u bytes), keeping old config\n", len);
        return;
    }
    state_update_home([](HomeState &st, void *ctx) {
        st.cfg = *static_cast<HomeConfig *>(ctx);
        st.have_config = true;
        return true;
    }, &cfg);
}

static void on_state(const char *topic, const uint8_t *payload, unsigned len) {
    SwitchState s{};
    if (!parse_switch_state(topic, (const char *)payload, len, s)) return;
    state_update_home([](HomeState &st, void *ctx) {
        const SwitchState &s = *static_cast<SwitchState *>(ctx);
        HomeSwitch *sw = find_switch(st.cfg, s.room, s.panel, s.id);
        if (!sw) return false;
        sw->on = s.on;
        sw->pending = false;
        return true;
    }, &s);
}

// ---------------- public ----------------

void home_service_init() {
    mqtt_service_route(MQTT_TOPIC_HOME_CONFIG, on_config);
    mqtt_service_route(MQTT_TOPIC_HOME_STATE, on_state);
}

struct SetReq {
    const char *room, *panel;
    uint16_t id;
    bool on;
    bool ok;
};

bool home_service_set(const char *room_id, const char *panel_id, uint16_t switch_id, bool on) {
    if (!mqtt_service_connected()) return false;
    SetReq req{room_id, panel_id, switch_id, on, false};
    state_update_home([](HomeState &st, void *ctx) {
        SetReq &q = *static_cast<SetReq *>(ctx);
        HomeSwitch *sw = find_switch(st.cfg, q.room, q.panel, q.id);
        if (!sw || sw->pending) return false;
        char topic[64];
        if (!build_switch_set_topic(topic, sizeof topic, q.room, q.panel, q.id)) return false;
        if (!mqtt_service_publish(topic, q.on ? "ON" : "OFF")) return false;   // only queues
        sw->pending = true;
        sw->pending_since_ms = millis();
        q.ok = true;
        return true;
    }, &req);
    return req.ok;
}

void home_service_task(void *) {
    for (;;) {
        const bool mqtt_up = mqtt_service_connected();
        state_update_home([](HomeState &st, void *ctx) {
            const bool up = *static_cast<bool *>(ctx);
            const uint32_t now = millis();
            // MQTT down: nothing will answer, drop in-flight taps. Otherwise revert
            // switches whose .../state never arrived.
            return for_each_switch(st.cfg, [&](HomeSwitch &sw) {
                if (!sw.pending) return false;
                if (up && now - sw.pending_since_ms < SWITCH_PENDING_MS) return false;
                sw.pending = false;
                return true;
            });
        }, (void *)&mqtt_up);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
