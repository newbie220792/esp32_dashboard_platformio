#include "ui_home.h"

#include <string.h>

#include "app_config.h"
#include "icon_draw.h"
#include "mqtt.h"
#include "ui_theme.h"

#define HEADER_H      32
#define NAV_BTN_W     44
#define CLUSTER_H     82     // 2 clusters fit; more scroll
#define CLUSTER_GAP   8
#define SWITCH_ICON   22
#define PENDING_TICK  200

struct SwitchView {
    lv_obj_t *btn, *icon, *name, *state;
    uint8_t panel, sw;
};

static HomeConfig *s_cfg = nullptr;
static int s_room = 0;
static bool s_mqtt_ok = false;

static lv_obj_t *s_title, *s_index, *s_prev, *s_next, *s_list;
static SwitchView s_views[HOME_MAX_PANELS * HOME_SWITCHES];
static int s_view_count = 0;

// ---------------- icons ----------------

static void switch_icon_draw_cb(lv_event_t *e) {
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

static lv_obj_t *switch_icon_create(lv_obj_t *parent, SwitchIcon icon) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, SWITCH_ICON, SWITCH_ICON);
    lv_obj_set_user_data(o, (void *)(uintptr_t)icon);
    lv_obj_add_event_cb(o, switch_icon_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

// ---------------- helpers ----------------

static HomeRoom *cur_room() {
    return (s_cfg && s_room < s_cfg->room_count) ? &s_cfg->rooms[s_room] : nullptr;
}

static void view_apply(SwitchView &v) {
    HomeRoom *room = cur_room();
    if (!room) return;
    const HomeSwitch &sw = room->panels[v.panel].switches[v.sw];
    if (!sw.present) return;

    lv_color_t fg = sw.on ? COL_ON_ACCENT : COL_TEXT;
    lv_color_t sub = sw.on ? COL_ON_ACCENT : COL_SW_OFF_FG;
    lv_obj_set_style_bg_color(v.btn, sw.on ? COL_SW_ON : COL_SW_OFF, 0);
    lv_obj_set_style_text_color(v.icon, sub, 0);
    lv_obj_invalidate(v.icon);
    lv_obj_set_style_text_color(v.name, fg, 0);
    lv_obj_set_style_text_color(v.state, sub, 0);

    // Pending: waiting for .../state. Offline: commands can't be sent.
    lv_opa_t opa = LV_OPA_COVER;
    if (sw.pending) opa = LV_OPA_40;
    else if (!s_mqtt_ok) opa = LV_OPA_50;
    lv_obj_set_style_opa(v.btn, opa, 0);
    lv_label_set_text(v.state, sw.pending ? "..." : (sw.on ? "ON" : "OFF"));
}

static void views_apply_all() {
    for (int i = 0; i < s_view_count; i++) view_apply(s_views[i]);
}

static void switch_clicked_cb(lv_event_t *e) {
    SwitchView &v = s_views[(uintptr_t)lv_event_get_user_data(e)];
    HomeRoom *room = cur_room();
    if (!room || !s_mqtt_ok) return;
    HomePanel &panel = room->panels[v.panel];
    HomeSwitch &sw = panel.switches[v.sw];
    if (!sw.present || sw.pending) return;
    if (!mqtt_request_switch(room->id, panel.id, sw.id, !sw.on)) return;
    sw.pending = true;
    sw.pending_since_ms = lv_tick_get();
    view_apply(v);
}

// ---------------- building ----------------

static void switch_create(lv_obj_t *row, uint8_t panel_idx, uint8_t sw_idx, const HomeSwitch &sw) {
    lv_obj_t *btn = ui_box(row);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    if (!sw.present || s_view_count >= (int)(sizeof s_views / sizeof s_views[0])) return;   // keeps the 1/3 slot

    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(btn, 4, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    SwitchView &v = s_views[s_view_count];
    v.btn = btn;
    v.panel = panel_idx;
    v.sw = sw_idx;
    v.icon = switch_icon_create(btn, sw.icon);
    v.name = ui_label(btn, FONT_M_BOLD, COL_TEXT, sw.name);
    lv_label_set_long_mode(v.name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(v.name, LV_PCT(100), 0);
    v.state = ui_label(btn, FONT_S, COL_SW_OFF_FG, "");
    lv_obj_add_event_cb(btn, switch_clicked_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)s_view_count);
    s_view_count++;
    view_apply(v);
}

static void cluster_create(uint8_t panel_idx, const HomePanel &panel) {
    lv_obj_t *box = ui_box(s_list);
    lv_obj_set_size(box, LV_PCT(100), CLUSTER_H);
    lv_obj_set_style_bg_color(box, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_pad_all(box, 4, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 2, 0);

    lv_obj_t *lbl = ui_label(box, FONT_S, COL_MUTED, panel.name);
    lv_obj_set_height(lbl, 14);
    lv_obj_set_style_pad_left(lbl, 2, 0);

    lv_obj_t *row = ui_box(box);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 4, 0);
    for (uint8_t i = 0; i < HOME_SWITCHES; i++) switch_create(row, panel_idx, i, panel.switches[i]);
}

static void nav_enable(lv_obj_t *btn, bool en) {
    if (en) lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(btn, en ? LV_OPA_COVER : LV_OPA_30, 0);
}

static void room_build() {
    lv_obj_clean(s_list);
    s_view_count = 0;
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);

    HomeRoom *room = cur_room();
    const int n = s_cfg ? s_cfg->room_count : 0;
    nav_enable(s_prev, n > 1);
    nav_enable(s_next, n > 1);

    if (!room) {
        lv_label_set_text(s_title, s_cfg ? "No rooms" : "Home");
        lv_label_set_text(s_index, "");
        ui_label(s_list, FONT_S, COL_MUTED, s_cfg ? "home/config has no rooms" : "Waiting for " MQTT_TOPIC_HOME_CONFIG "...");
        return;
    }
    lv_label_set_text(s_title, room->name);
    lv_label_set_text_fmt(s_index, "%d/%d", s_room + 1, n);
    if (!room->panel_count) ui_label(s_list, FONT_S, COL_MUTED, "No panels in this room");
    for (uint8_t p = 0; p < room->panel_count; p++) cluster_create(p, room->panels[p]);
}

static void nav_cb(lv_event_t *e) {
    if (!s_cfg || s_cfg->room_count < 2) return;
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    s_room = (s_room + dir + s_cfg->room_count) % s_cfg->room_count;
    room_build();
}

static lv_obj_t *nav_btn_create(lv_obj_t *parent, const char *symbol, int dir, lv_align_t align) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, NAV_BTN_W, HEADER_H);
    lv_obj_align(b, align, 0, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, COL_PANEL, 0);
    lv_obj_set_style_bg_color(b, COL_SW_OFF, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = ui_label(b, FONT_MD, COL_TEXT, symbol);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
    return b;
}

// Revert switches whose .../state never arrived.
static void pending_timer_cb(lv_timer_t *) {
    if (!s_cfg) return;
    bool changed = false;
    for (int r = 0; r < s_cfg->room_count; r++)
        for (int p = 0; p < s_cfg->rooms[r].panel_count; p++)
            for (HomeSwitch &sw : s_cfg->rooms[r].panels[p].switches)
                if (sw.pending && lv_tick_elaps(sw.pending_since_ms) >= SWITCH_PENDING_MS) {
                    sw.pending = false;
                    changed |= (r == s_room);
                }
    if (changed) views_apply_all();
}

// ---------------- public ----------------

void ui_home_create(lv_obj_t *tile) {
    lv_obj_t *card = ui_card(tile, 6);
    lv_obj_set_size(card, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, CLUSTER_GAP, 0);

    // Header: ‹  Room name  1/4  ›   (stays put while the list scrolls)
    lv_obj_t *hdr = ui_box(card);
    lv_obj_set_size(hdr, LV_PCT(100), HEADER_H);
    s_prev = nav_btn_create(hdr, LV_SYMBOL_LEFT, -1, LV_ALIGN_LEFT_MID);
    s_next = nav_btn_create(hdr, LV_SYMBOL_RIGHT, +1, LV_ALIGN_RIGHT_MID);
    lv_obj_t *mid = ui_box(hdr);
    lv_obj_set_size(mid, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mid, 6, 0);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 0);
    s_title = ui_label(mid, FONT_L_BOLD, COL_TEXT, "Home");
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s_title, 300, 0);
    s_index = ui_label(mid, FONT_S, COL_MUTED, "");

    // Clusters (scrolls vertically; horizontal drags chain to the tileview)
    s_list = ui_box(card);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, CLUSTER_GAP, 0);
    lv_obj_set_style_pad_right(s_list, 5, 0);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    ui_style_scrollbar(s_list);

    room_build();
    lv_timer_create(pending_timer_cb, PENDING_TICK, nullptr);
}

void ui_home_set_config(HomeConfig *cfg) {
    // Stay on the same room if it still exists.
    char keep_id[HOME_ID_LEN] = "";
    if (HomeRoom *r = cur_room()) strcpy(keep_id, r->id);
    delete s_cfg;
    s_cfg = cfg;
    s_room = 0;
    for (int i = 0; i < s_cfg->room_count; i++)
        if (!strcmp(s_cfg->rooms[i].id, keep_id)) s_room = i;
    room_build();
}

void ui_home_apply_state(const SwitchState &st) {
    if (!s_cfg) return;
    for (int r = 0; r < s_cfg->room_count; r++) {
        HomeRoom &room = s_cfg->rooms[r];
        if (strcmp(room.id, st.room)) continue;
        for (int p = 0; p < room.panel_count; p++) {
            HomePanel &panel = room.panels[p];
            if (strcmp(panel.id, st.panel)) continue;
            for (HomeSwitch &sw : panel.switches) {
                if (!sw.present || sw.id != st.id) continue;
                sw.on = st.on;
                sw.pending = false;
                if (r == s_room) views_apply_all();
                return;
            }
        }
    }
}

void ui_home_set_mqtt(bool connected) {
    s_mqtt_ok = connected;
    if (!connected && s_cfg) {
        // Nothing will answer now; drop any in-flight taps.
        for (int r = 0; r < s_cfg->room_count; r++)
            for (int p = 0; p < s_cfg->rooms[r].panel_count; p++)
                for (HomeSwitch &sw : s_cfg->rooms[r].panels[p].switches) sw.pending = false;
    }
    views_apply_all();
}
