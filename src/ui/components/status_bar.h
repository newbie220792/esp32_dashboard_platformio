#pragma once
// Status bar (28px) over the tileview:
//   dot + title | page pills | bell (red dot) + gear + Wi-Fi icon + clock.
// One bar is shared by all tiles; ui_manager sets it for the active tile.
#include <lvgl.h>

struct StatusBarActions {
    void (*on_wifi_tap)();       // Wi-Fi icon (reopen the setup screens)
    void (*on_alerts_tap)();     // bell
    void (*on_settings_tap)();   // gear
};

void status_bar_create(lv_obj_t *scr, int page_count, const StatusBarActions &actions);
void status_bar_set_page(int active);
void status_bar_set_title(const char *title);   // copied; no-op when unchanged
void status_bar_set_dot(bool ok);    // green #3DDC84 / gray #6B7788
void status_bar_set_wifi(bool ok);   // same colours; never hidden
void status_bar_set_alerts(bool show_badge);    // red dot on the bell
void status_bar_set_visible(bool visible);      // hidden on the Photos tab (full-screen photo)
