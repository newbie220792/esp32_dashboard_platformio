#pragma once
// Small 270° arc gauge in a card: value inside the arc (14), caption below (12).
#include <lvgl.h>

struct Gauge {
    lv_obj_t *arc;
    lv_obj_t *value;
    lv_obj_t *caption;
};

// Card grows to share the row with its siblings; range defaults to 0..100.
void gauge_create(Gauge &g, lv_obj_t *row, lv_color_t color, const char *caption);
void gauge_set(Gauge &g, int32_t arc_value, const char *text);   // value text turns normal colour
void gauge_set_stale(Gauge &g);                                    // value text grays out
