#include "ui/components/overlay.h"

#include "ui/theme.h"

#define BACK_H 32

static void back_cb(lv_event_t *e) {
    auto fn = reinterpret_cast<void (*)()>(lv_event_get_user_data(e));
    if (fn) fn();
}

void overlay_create(Overlay &o, const char *title, void (*on_back)()) {
    o.scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(o.scr);
    lv_obj_set_style_bg_color(o.scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(o.scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(o.scr, COL_TEXT, 0);
    lv_obj_set_style_text_font(o.scr, FONT_M, 0);
    lv_obj_remove_flag(o.scr, LV_OBJ_FLAG_SCROLLABLE);

    // Top bar (same look as the Wi-Fi screens): "< Back", title, right items.
    o.bar = ui_box(o.scr);
    lv_obj_set_size(o.bar, LV_PCT(100), OVERLAY_BAR_H);
    lv_obj_set_style_pad_hor(o.bar, UI_PAD, 0);
    lv_obj_set_style_pad_top(o.bar, 4, 0);
    lv_obj_set_flex_flow(o.bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o.bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o.bar, 10, 0);

    lv_obj_t *back = ui_box(o.bar);
    lv_obj_set_size(back, LV_SIZE_CONTENT, BACK_H);
    lv_obj_set_style_pad_hor(back, 12, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_set_style_bg_color(back, COL_CARD, 0);
    lv_obj_set_style_bg_color(back, COL_SW_OFF, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back, 4);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(on_back));
    lv_obj_t *back_l = ui_label(back, FONT_M_BOLD, COL_TEXT, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_l);

    lv_obj_t *t = ui_label(o.bar, FONT_M_BOLD, COL_TEXT, title);
    lv_obj_set_flex_grow(t, 1);

    o.content = ui_box(o.scr);
    lv_obj_set_size(o.content, LV_PCT(100), lv_display_get_vertical_resolution(nullptr) - OVERLAY_BAR_H);
    lv_obj_align(o.content, LV_ALIGN_TOP_LEFT, 0, OVERLAY_BAR_H);
    lv_obj_set_style_pad_all(o.content, UI_PAD, 0);
    lv_obj_set_style_pad_top(o.content, UI_GAP, 0);
    lv_obj_set_flex_flow(o.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o.content, UI_GAP, 0);
}
