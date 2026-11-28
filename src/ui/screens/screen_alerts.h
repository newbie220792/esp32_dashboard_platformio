#pragma once
// Alerts overlay (task 8, artboard 8): up to 10 alerts from the Pi 5, newest
// first, "Clear all". Row widgets exist only while the screen is open.
#include <lvgl.h>

lv_obj_t *screen_alerts_create(void (*on_back)());   // returns the lv_screen
void screen_alerts_open();    // before lv_screen_load: starts the fetch
void screen_alerts_close();   // after leaving: deletes the rows, keeps the data
void screen_alerts_poll();    // redraw from StateGroup::Alerts while open
