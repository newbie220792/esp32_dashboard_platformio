#pragma once
// System info overlay (task 9, artboard 9): Network + Device cards, read-only.
// Opened from the Settings row, Back returns to Settings. Unlike Alerts and
// Settings the screen exists only while shown: built by screen_sysinfo_create(),
// deleted when left (lv_screen_load_anim auto_del, or screen_sysinfo_drop()).
// Its 1 s lv_timer lives and dies with it.
#include <lvgl.h>

lv_obj_t *screen_sysinfo_create(void (*on_back)());   // returns the new lv_screen
lv_obj_t *screen_sysinfo_get();                       // nullptr when not built
void screen_sysinfo_drop();   // delete it if it exists and is not the active screen
void screen_sysinfo_poll();   // redraw at once on a Net / ApiHealth change (while built)
