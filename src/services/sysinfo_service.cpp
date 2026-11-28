#include "services/sysinfo_service.h"

#include <Arduino.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string.h>

#include "app_config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "hal/sysinfo.h"
#include "services/pi5_api.h"
#include "services/time_service.h"
#include "services/wifi_service.h"

#define BIT_WAKE    (1 << 0)   // open state changed
#define MAX_WAIT_MS 500        // while open: notices Wi-Fi going / coming back

#define PATH_HEALTH   "/api/health"
#define PATH_FALLBACK "/api/alerts/summary"   // task 8 endpoint, for a Pi 5 without /api/health

static EventGroupHandle_t s_ev;
static volatile bool s_open = false;
static volatile uint32_t s_sketch_size = 0;   // set once by the task
static uint32_t s_app_size;                   // partition table: fixed after init

void sysinfo_service_init() {
    s_ev = xEventGroupCreate();
    s_app_size = hal_sys_app_partition_size();
}

void sysinfo_service_set_open(bool open) {
    s_open = open;
    if (s_ev) xEventGroupSetBits(s_ev, BIT_WAKE);
}

void sysinfo_service_read(SysInfo &out) {
    out = SysInfo{};
    const NetState ns = state_get_net();
    out.wifi_connected = ns.wifi_connected;
    out.ip = ns.ip;
    out.rssi = ns.rssi;
    out.mqtt_connected = ns.mqtt_connected;
    if (ns.wifi_connected) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            ap.ssid[sizeof ap.ssid - 1] = 0;
            ascii_sanitize(out.ssid, sizeof out.ssid, (const char *)ap.ssid);
            out.rssi = ap.rssi;   // fresher than the 2 s NetState copy
        }
    }

    const ApiHealthState h = state_get_api_health();
    out.api = h.status;
    out.api_rtt_ms = h.rtt_ms;

    out.time_synced = time_service_synced();
    out.last_sync = time_service_last_sync();

    strncpy(out.fw_version, FW_VERSION, sizeof out.fw_version - 1);
    out.uptime_s = hal_sys_uptime_s();
    out.heap_free = hal_sys_heap_free();
    out.heap_total = hal_sys_heap_total();
    out.psram_free = hal_sys_psram_free();
    out.psram_total = hal_sys_psram_total();
    out.sketch_size = s_sketch_size;
    out.app_size = s_app_size;
}

// Same numbers as the screen, for checking against the serial log.
static void log_device() {
    Serial.printf("[sysinfo] fw v%s, heap %u / %u, psram %u / %u, sketch %u, app partition %u\n",
                  FW_VERSION, (unsigned)hal_sys_heap_free(), (unsigned)hal_sys_heap_total(),
                  (unsigned)hal_sys_psram_free(), (unsigned)hal_sys_psram_total(), (unsigned)s_sketch_size,
                  (unsigned)s_app_size);
}

// ---------------- health check (sysinfo task only) ----------------

static Pi5Api s_api;
static bool s_use_fallback = false;   // the Pi 5 answered 404 to /api/health

static int timed_get(const char *path, uint32_t &rtt_ms) {
    String body;
    const uint32_t t0 = millis();
    const int code = pi5_api_get(s_api, path, body);
    rtt_ms = millis() - t0;
    return code;
}

static void check_health() {
    uint32_t rtt = 0;
    int code = timed_get(s_use_fallback ? PATH_FALLBACK : PATH_HEALTH, rtt);
    if (code == 404 && !s_use_fallback) {
        s_use_fallback = true;
        Serial.println("[sysinfo] Pi 5 has no " PATH_HEALTH ", timing " PATH_FALLBACK " instead");
        code = timed_get(PATH_FALLBACK, rtt);
    }
    if (code == 200) state_set_api_health(ApiHealth::Ok, (uint16_t)min<uint32_t>(rtt, 65535));
    else state_set_api_health(ApiHealth::Error, 0);
}

static void drop_connection() {
    s_api.http.end();
    s_api.tcp.stop();
}

// ---------------- task ----------------

static bool due(uint32_t at) { return (int32_t)(millis() - at) >= 0; }

void sysinfo_service_task(void *) {
    s_sketch_size = hal_sys_sketch_size();   // ~tens of ms: here, not on the LVGL task
    log_device();
    s_api.timeout_ms = SYSINFO_API_TIMEOUT_MS;

    bool was_open = false;
    uint32_t next_check = 0;
    for (;;) {
        // Closed: sleep until the screen opens (no CPU, no requests).
        const TickType_t wait = was_open ? pdMS_TO_TICKS(MAX_WAIT_MS) : portMAX_DELAY;
        xEventGroupWaitBits(s_ev, BIT_WAKE, pdTRUE, pdFALSE, wait);

        const bool open = s_open;
        if (open && !was_open) {
            log_device();
            state_set_api_health(pi5_api_configured() ? ApiHealth::Unknown : ApiHealth::NotConfigured, 0);
            next_check = millis();
        }
        if (!open && was_open) drop_connection();   // free the socket while nobody looks
        was_open = open;
        if (!open || !pi5_api_configured()) continue;

        if (!wifi_service_is_connected()) {
            state_set_api_health(ApiHealth::Unknown, 0);   // "--": not tried
            next_check = millis();                         // check as soon as the IP is back
            continue;
        }
        if (due(next_check)) {
            check_health();   // blocks this task only, at most SYSINFO_API_TIMEOUT_MS per try
            next_check = millis() + SYSINFO_API_MS;
        }
    }
}
