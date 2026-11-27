#pragma once
// Settings overlay (task 8, artboard 7): Brightness, Screen off, Temperature,
// Location (read-only, from the Pi 5). Changes go to settings_service at once.
#include <lvgl.h>

lv_obj_t *screen_settings_create(void (*on_back)());   // returns the lv_screen
void screen_settings_poll();                           // redraw from Settings / Location state
