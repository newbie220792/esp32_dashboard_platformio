#include "services/time_service.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <time.h>

#include "app_config.h"
#include "services/wifi_service.h"

static volatile uint32_t s_last_sync = 0;

// lwIP task: keep it to a store. SNTP calls it again on every re-sync (hourly).
static void on_sync(struct timeval *tv) { s_last_sync = tv ? (uint32_t)tv->tv_sec : (uint32_t)time(nullptr); }

void time_service_init() { sntp_set_time_sync_notification_cb(on_sync); }

void time_service_task(void *) {
    wifi_service_wait_connected(portMAX_DELAY);
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
    Serial.println("[time] SNTP started");
    vTaskDelete(nullptr);
}

bool time_service_synced() { return time(nullptr) > 1700000000; }   // after Nov 2023

uint32_t time_service_last_sync() { return s_last_sync; }
