#pragma once
// Root UI: status bar + tileview (one tile per screen_*), the Wi-Fi setup
// screen, and the LVGL task. The UI reads the state store and sends commands
// through services; it never touches hal/.

// Builds every screen. Call once after hal_display_init(), before ui_task runs.
void ui_init();
// The LVGL task (created by app.cpp): the only caller of lv_timer_handler().
// Drains core/events and polls the state store.
void ui_task(void *arg);
