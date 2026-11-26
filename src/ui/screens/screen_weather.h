#pragma once
#include <lvgl.h>

// Tile 2: weather. LVGL task only.
// On a failed fetch the last data stays with "Updated N min ago"; with no data
// yet the card shows "Weather unavailable" + the reason.
void screen_weather_create(lv_obj_t *tile);
void screen_weather_poll();   // redraws from the state store when it changed
bool screen_weather_ok();     // last fetch succeeded (status-bar dot)
