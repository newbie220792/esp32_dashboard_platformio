#pragma once
#include <lvgl.h>
#include "app_events.h"

// Tile 2: weather. LVGL task only.
void ui_weather_create(lv_obj_t *tile);
void ui_weather_update(const WeatherData &w);
// A fetch failed: keep the last data and show its age, or, if there never was
// any, "Weather unavailable" + the reason. Only the weather card changes.
void ui_weather_set_error(WeatherErr e);
