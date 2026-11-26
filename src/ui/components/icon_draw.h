#pragma once
// Tiny helpers for flat vector icons drawn in an LV_EVENT_DRAW_MAIN callback.
// Coordinates are in 0..100 units of the icon's square box.
#include <lvgl.h>

struct IconCtx {
    lv_layer_t *layer;
    int32_t x0, y0, s;
    lv_color_t bg;   // nearest opaque ancestor colour (for cut-outs)
};

IconCtx icon_ctx(lv_event_t *e);
void icon_circle(const IconCtx &c, int32_t cx, int32_t cy, int32_t r, lv_color_t col);
void icon_rrect(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t radius,
                lv_color_t col);
void icon_line(const IconCtx &c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w, lv_color_t col);
// Arc around (cx, cy); angles in degrees, LVGL convention (0 = 3 o'clock, clockwise).
void icon_arc(const IconCtx &c, int32_t cx, int32_t cy, int32_t r, int32_t w, int32_t start, int32_t end,
              lv_color_t col);
