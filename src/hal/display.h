#pragma once
#include <lvgl.h>

// NV3041A (GFX Library for Arduino) + LVGL display driver: lv_init(), tick,
// PSRAM draw buffers, flush callback. Call once before the LVGL task starts.
lv_display_t *hal_display_init();
