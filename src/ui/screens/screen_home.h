#pragma once
#include <lvgl.h>

// Tile 3: home switches, room -> panels -> 3 switches. LVGL task only.
// Taps go to home_service_set(); colours change only when the state store does.
void screen_home_create(lv_obj_t *tile);
void screen_home_poll();   // redraws from the state store when it changed
