#include "hal/sysinfo.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>

uint64_t hal_sys_uptime_s() { return (uint64_t)esp_timer_get_time() / 1000000ULL; }

uint32_t hal_sys_heap_free() { return ESP.getFreeHeap(); }
uint32_t hal_sys_heap_total() { return ESP.getHeapSize(); }
uint32_t hal_sys_psram_free() { return ESP.getFreePsram(); }
uint32_t hal_sys_psram_total() { return ESP.getPsramSize(); }

uint32_t hal_sys_sketch_size() { return ESP.getSketchSize(); }

uint32_t hal_sys_app_partition_size() {
    const esp_partition_t *p = esp_ota_get_running_partition();
    return p ? p->size : 0;
}

// huge_app.csv has a single app slot (ota_0): the "next" slot is then the
// running one, which is no OTA at all.
uint32_t hal_sys_ota_slot_size() {
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    const esp_partition_t *run = esp_ota_get_running_partition();
    return next && (!run || next->address != run->address) ? next->size : 0;
}
