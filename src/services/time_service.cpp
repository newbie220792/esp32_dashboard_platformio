#include "services/time_service.h"

#include <Arduino.h>
#include <time.h>

#include "app_config.h"
#include "services/wifi_service.h"

void time_service_init() {}

void time_service_task(void *) {
    wifi_service_wait_connected(portMAX_DELAY);
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
    Serial.println("[time] SNTP started");
    vTaskDelete(nullptr);
}

bool time_service_synced() { return time(nullptr) > 1700000000; }   // after Nov 2023
