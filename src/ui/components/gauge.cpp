#include "ui/components/gauge.h"

#include "ui/theme.h"

#define ARC_SIZE  56
#define ARC_WIDTH 6

void gauge_create(Gauge &g, lv_obj_t *row, lv_color_t color, const char *caption) {
    lv_obj_t *card = ui_card(row, 4);
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 1, 0);

    g.arc = lv_arc_create(card);
    lv_obj_set_size(g.arc, ARC_SIZE, ARC_SIZE);
    lv_arc_set_rotation(g.arc, 135);
    lv_arc_set_bg_angles(g.arc, 0, 270);
    lv_arc_set_range(g.arc, 0, 100);
    lv_arc_set_value(g.arc, 0);
    lv_obj_remove_style(g.arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(g.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(g.arc, 0, 0);
    lv_obj_set_style_arc_width(g.arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g.arc, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g.arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g.arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_INDICATOR);

    g.value = ui_label(g.arc, FONT_M, COL_MUTED, "--");
    lv_obj_center(g.value);

    g.caption = ui_label(card, FONT_S, COL_MUTED, caption);
}

void gauge_set(Gauge &g, int32_t arc_value, const char *text) {
    lv_arc_set_value(g.arc, arc_value);
    lv_label_set_text(g.value, text);
    lv_obj_set_style_text_color(g.value, COL_TEXT, 0);
}

void gauge_set_stale(Gauge &g) { lv_obj_set_style_text_color(g.value, COL_MUTED, 0); }
