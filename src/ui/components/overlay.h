#pragma once
// Full-screen overlay pages (Alerts, Settings): their own lv_screen, outside
// the swipe strip. 36px top bar with "< Back" + title; content area below.
#include <lvgl.h>

#define OVERLAY_BAR_H 36

struct Overlay {
    lv_obj_t *scr;
    lv_obj_t *bar;       // flex row: Back, title, then a growing spacer; add right-side items here
    lv_obj_t *content;   // below the bar, padded like a tile (10px), flex column, gap 8
};

void overlay_create(Overlay &o, const char *title, void (*on_back)());
