#pragma once
#include <lvgl.h>
#include "app_events.h"

// Tile 1: Pi 5 monitor. LVGL task only.
void ui_monitor_create(lv_obj_t *tile);
void ui_monitor_update(const PiStats &s);
