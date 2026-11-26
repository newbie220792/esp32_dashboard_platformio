#include "ui/theme.h"

lv_obj_t *ui_box(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *ui_card(lv_obj_t *parent, int32_t pad) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_style_bg_color(o, COL_CARD, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, UI_RADIUS, 0);
    lv_obj_set_style_pad_all(o, pad, 0);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

void ui_style_scrollbar(lv_obj_t *obj) {
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(obj, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(obj, COL_SCROLLBAR, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(obj, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_ver(obj, 0, LV_PART_SCROLLBAR);
}
