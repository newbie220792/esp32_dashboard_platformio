#pragma once
// Chip / system readings for the System info screen (task 9). All cheap and
// callable from any task, except hal_sys_sketch_size() (see below).
#include <stdint.h>

uint64_t hal_sys_uptime_s();         // esp_timer (64-bit), no millis() wrap
uint32_t hal_sys_heap_free();        // internal RAM, ESP.getFreeHeap()
uint32_t hal_sys_heap_total();       // ESP.getHeapSize()
uint32_t hal_sys_psram_free();       // 0 when there is no PSRAM
uint32_t hal_sys_psram_total();
// ESP.getSketchSize(): verifies (hashes) the whole app image, tens of ms.
// Call once from a background task, never from the LVGL task.
uint32_t hal_sys_sketch_size();
uint32_t hal_sys_app_partition_size();   // the running app partition
