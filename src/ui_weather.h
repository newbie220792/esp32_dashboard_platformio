#pragma once
#include <lvgl.h>
#include "app_events.h"

// Tile 2: weather. LVGL task only.
void ui_weather_create(lv_obj_t *tile);
void ui_weather_update(const WeatherData &w);
