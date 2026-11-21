#pragma once
#include <lvgl.h>
#include "models.h"

// Tile 1: Pi 5 monitor (gauges + service list). LVGL task only.
void ui_monitor_create(lv_obj_t *tile);
void ui_monitor_update(const PiStats &s);
void ui_monitor_set_services(const ServiceList &list);
