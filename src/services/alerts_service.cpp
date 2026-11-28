#include "services/alerts_service.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "app_config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/pi5_api.h"
#include "services/wifi_service.h"

#define BIT_WAKE    (1 << 0)   // open state changed
#define BIT_REFRESH (1 << 1)
#define BIT_CLEAR   (1 << 2)
#define MAX_WAIT_MS 1000       // also notices Wi-Fi coming back

static EventGroupHandle_t s_ev;
static volatile bool s_open = false;

void alerts_service_init() { s_ev = xEventGroupCreate(); }

void alerts_service_set_open(bool open) {
    s_open = open;
    if (s_ev) xEventGroupSetBits(s_ev, BIT_WAKE);
}

void alerts_service_refresh() {
    if (s_ev) xEventGroupSetBits(s_ev, BIT_REFRESH);
}

void alerts_service_clear_all() {
    if (s_ev) xEventGroupSetBits(s_ev, BIT_CLEAR);
}

// ---------------- requests (alerts task only) ----------------

static Pi5Api s_api;

static bool fetch_summary(AlertSummary &out) {
    String body;
    if (pi5_api_get(s_api, "/api/alerts/summary", body) != 200) return false;
    if (!parse_alerts_summary(body.c_str(), body.length(), out)) {
        Serial.println("[alerts] bad summary response");
        return false;
    }
    state_set_alerts_summary(out);
    return true;
}

static bool fetch_list() {
    state_set_alerts_op(AlertsOp::Loading);
    String body;
    // list kept in a static: AlertList is ~1.3 KB, the task stack stays small
    static AlertList list;
    const uint32_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (pi5_api_get(s_api, "/api/alerts", body) == 200 && parse_alerts(body.c_str(), body.length(), list)) {
        state_set_alerts_list(list);
        Serial.printf("[alerts] %u alerts (%u bytes), internal heap %u -> %u\n", list.count, body.length(),
                      (unsigned)before, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        return true;
    }
    state_set_alerts_error(AlertsErr::LoadFailed);
    return false;
}

static bool clear_all() {
    state_set_alerts_op(AlertsOp::Clearing);
    String body;
    const int code = pi5_api_post(s_api, "/api/alerts/ack-all", body);
    if (code == 200) {
        state_alerts_cleared();
        Serial.println("[alerts] cleared");
        return true;
    }
    state_set_alerts_error(AlertsErr::ClearFailed);
    return false;
}

// ---------------- task ----------------

static bool due(uint32_t at) { return (int32_t)(millis() - at) >= 0; }

void alerts_service_task(void *) {
    uint32_t next_summary = 0, next_list = 0;
    uint32_t backoff = ALERTS_SUMMARY_RETRY_MS;
    uint32_t summary_id = 0;       // latest_id of the last good summary
    uint32_t list_latest_id = 0;   // summary_id when the list was last loaded
    bool was_open = false, was_up = false;

    if (!pi5_api_configured()) Serial.println("[alerts] PI5_API_HOST not set: alerts stay offline");

    for (;;) {
        // Without an API host every request fails at once: treat it as offline
        // (badge hidden, the Alerts screen shows the error + Retry).
        const bool up = wifi_service_is_connected() && pi5_api_configured();
        const bool open = s_open;
        EventBits_t bits = xEventGroupClearBits(s_ev, BIT_WAKE | BIT_REFRESH | BIT_CLEAR);   // returns the bits before clearing

        if (up && !was_up) {   // boot or network back: refresh at once
            next_summary = millis();
            backoff = ALERTS_SUMMARY_RETRY_MS;
        }
        was_up = up;
        if (open && !was_open) bits |= BIT_REFRESH;   // screen just opened
        was_open = open;

        if (bits & BIT_CLEAR) {
            if (up) {
                if (clear_all()) next_summary = millis();
            } else {
                state_set_alerts_error(AlertsErr::ClearFailed);
            }
        }

        if (open && ((bits & BIT_REFRESH) || due(next_list))) {
            if (up) {
                fetch_list();
                list_latest_id = summary_id;
            } else {
                state_set_alerts_error(AlertsErr::LoadFailed);   // keep rows, show Retry
            }
            next_list = millis() + ALERTS_LIST_MS;
        }

        if (up && due(next_summary)) {
            AlertSummary s{};
            if (fetch_summary(s)) {
                summary_id = s.latest_id;
                backoff = ALERTS_SUMMARY_RETRY_MS;
                next_summary = millis() + ALERTS_SUMMARY_MS;
                if (open && s.latest_id != list_latest_id) next_list = millis();   // new alert: reload now
            } else {
                Serial.printf("[alerts] summary failed, retry in %lus\n", (unsigned long)(backoff / 1000));
                next_summary = millis() + backoff;
                backoff = min<uint32_t>(backoff * 2, ALERTS_SUMMARY_MAX_MS);
            }
        }

        // Sleep until the next deadline, a command, or MAX_WAIT_MS.
        uint32_t wait = MAX_WAIT_MS;
        if (up) {
            const uint32_t now = millis();
            const uint32_t to_summary = due(next_summary) ? 0 : next_summary - now;
            wait = min(wait, to_summary);
            if (open) wait = min(wait, due(next_list) ? 0u : next_list - now);
        }
        if (wait) xEventGroupWaitBits(s_ev, BIT_WAKE | BIT_REFRESH | BIT_CLEAR, pdFALSE, pdFALSE, pdMS_TO_TICKS(wait));
    }
}
