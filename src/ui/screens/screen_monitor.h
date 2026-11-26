#pragma once
#include <lvgl.h>

// Tile 1: Pi 5 monitor (3 gauges + service list). LVGL task only.
void screen_monitor_create(lv_obj_t *tile);
void screen_monitor_poll();   // redraws from the state store when it changed
