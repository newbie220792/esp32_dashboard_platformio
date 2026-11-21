#pragma once
// Root UI: status bar + 3-tile tileview. Every function here must run on the
// LVGL task.

void ui_init();
void ui_process_events();   // drain app event queue into widgets
void ui_next_tile();        // nav button / auto-rotate
void ui_wifi_back();        // Wi-Fi screens "Back": show the main UI (tile 1), setup keeps running
void ui_wifi_reopen();      // show the Wi-Fi screens again (status-bar icon, BOOT long-press)
