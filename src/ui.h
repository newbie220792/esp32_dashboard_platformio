#pragma once
// Root UI: status bar + 2-tile tileview. Every function here must run on the
// LVGL task.

void ui_init();
void ui_process_events();   // drain app event queue into widgets
void ui_next_tile();        // nav button / auto-rotate
