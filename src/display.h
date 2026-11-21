#pragma once
#include <lvgl.h>

// Initialise LovyanGFX + LVGL display driver. Call once from setup(),
// before the LVGL task starts.
lv_display_t *display_init();
