#pragma once
#include <lvgl.h>

// Tile 5: Photos slideshow (task 10). Full 480x272 photo, no status bar; a tap
// shows the controls (top: "Photos", page dots, Wi-Fi, clock; bottom: 5s/10s,
// Prev, Pause/Play, Next, "3 / 24"), which hide again after 3 s. Photos change
// only by Prev/Next or the timer: horizontal swipes still change tabs.
// LVGL task only.
void screen_photos_create(lv_obj_t *tile);
void screen_photos_poll();   // swaps in a new frame (300 ms fade) when the service has one
// ui_manager: whether the tab is what the user sees now (tile active, main
// screen loaded). Tells photo_service to start / stop and hides the controls.
void screen_photos_set_visible(bool visible);
void screen_photos_set_pages(int count, int active);   // page dots in the controls bar
