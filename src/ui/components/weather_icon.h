#pragma once
#include <lvgl.h>

// Flat vector weather icon drawn with lv_draw_* (no bitmaps), any size.
lv_obj_t *weather_icon_create(lv_obj_t *parent, int32_t size);
void weather_icon_set(lv_obj_t *icon, int wmo_code, bool is_day);

// Short English description of a WMO weather code.
const char *weather_code_text(int wmo_code);
