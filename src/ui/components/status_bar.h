#pragma once
// Status bar (28px) over the tileview: dot + title | page pills | Wi-Fi icon + clock.
// One bar is shared by all tiles; ui_manager sets it for the active tile.
#include <lvgl.h>

// `on_wifi_tap` runs when the Wi-Fi icon is tapped (reopen the setup screens).
void status_bar_create(lv_obj_t *scr, int page_count, void (*on_wifi_tap)());
void status_bar_set_page(int active);
void status_bar_set_title(const char *title);   // static string (compared by pointer)
void status_bar_set_dot(bool ok);    // green #3DDC84 / gray #6B7788
void status_bar_set_wifi(bool ok);   // same colours; never hidden
