#include "ui/components/switch_tile.h"

#include "ui/components/icon_draw.h"
#include "ui/theme.h"

#define SWITCH_ICON 22

static void icon_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    IconCtx c = icon_ctx(e);
    lv_color_t col = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    switch ((SwitchIcon)(uintptr_t)lv_obj_get_user_data(obj)) {
    case SwitchIcon::Bulb:
        icon_circle(c, 50, 38, 28, col);
        icon_rrect(c, 36, 62, 64, 76, 4, col);
        icon_line(c, 40, 88, 60, 88, 9, col);
        break;
    case SwitchIcon::Fan:
        icon_circle(c, 50, 24, 18, col);
        icon_circle(c, 27, 63, 18, col);
        icon_circle(c, 73, 63, 18, col);
        icon_circle(c, 50, 50, 14, c.bg);
        icon_circle(c, 50, 50, 8, col);
        break;
    case SwitchIcon::Plug:
        icon_line(c, 38, 10, 38, 30, 10, col);
        icon_line(c, 62, 10, 62, 30, 10, col);
        icon_rrect(c, 22, 30, 78, 70, 18, col);
        icon_line(c, 50, 70, 50, 92, 10, col);
        break;
    case SwitchIcon::Snow:
        icon_line(c, 50, 8, 50, 92, 9, col);
        icon_line(c, 14, 29, 86, 71, 9, col);
        icon_line(c, 14, 71, 86, 29, 9, col);
        break;
    }
}

static lv_obj_t *icon_create(lv_obj_t *parent, SwitchIcon icon) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, SWITCH_ICON, SWITCH_ICON);
    lv_obj_set_user_data(o, (void *)(uintptr_t)icon);
    lv_obj_add_event_cb(o, icon_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

bool switch_tile_create(SwitchTile &t, lv_obj_t *row, const HomeSwitch &sw, lv_event_cb_t on_click, void *user) {
    lv_obj_t *btn = ui_box(row);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    if (!sw.present) return false;   // keeps the 1/3 slot

    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(btn, 4, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    t.btn = btn;
    t.icon = icon_create(btn, sw.icon);
    t.name = ui_label(btn, FONT_M_BOLD, COL_TEXT, sw.name);
    lv_label_set_long_mode(t.name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(t.name, LV_PCT(100), 0);
    t.state = ui_label(btn, FONT_S, COL_SW_OFF_FG, "");
    lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, user);
    return true;
}

void switch_tile_apply(SwitchTile &t, const HomeSwitch &sw, bool mqtt_ok) {
    if (!sw.present) return;
    lv_color_t fg = sw.on ? COL_ON_ACCENT : COL_TEXT;
    lv_color_t sub = sw.on ? COL_ON_ACCENT : COL_SW_OFF_FG;
    lv_obj_set_style_bg_color(t.btn, sw.on ? COL_SW_ON : COL_SW_OFF, 0);
    lv_obj_set_style_text_color(t.icon, sub, 0);
    lv_obj_invalidate(t.icon);
    lv_obj_set_style_text_color(t.name, fg, 0);
    lv_obj_set_style_text_color(t.state, sub, 0);

    // Pending: waiting for .../state. Offline: commands can't be sent.
    lv_opa_t opa = LV_OPA_COVER;
    if (sw.pending) opa = LV_OPA_40;
    else if (!mqtt_ok) opa = LV_OPA_50;
    lv_obj_set_style_opa(t.btn, opa, 0);
    lv_label_set_text(t.state, sw.pending ? "..." : (sw.on ? "ON" : "OFF"));
}
