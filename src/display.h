#pragma once
#include <lvgl.h>

// Initialise the NV3041A (GFX Library for Arduino) + LVGL display driver. Call once from setup(),
// before the LVGL task starts.
lv_display_t *display_init();
