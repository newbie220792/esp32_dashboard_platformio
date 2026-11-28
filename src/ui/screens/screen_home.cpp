#include "ui/screens/screen_home.h"

#include <string.h>

#include "app_config.h"
#include "core/state.h"
#include "services/home_service.h"
#include "ui/components/switch_tile.h"
#include "ui/theme.h"

#define HEADER_H      32
#define NAV_BTN_W     44
#define CLUSTER_H     82     // 2 clusters fit; more scroll
#define CLUSTER_GAP   8

struct SwitchView {
    SwitchTile tile;
    uint8_t panel, sw;
};

// Last copy drawn and the incoming one (LVGL task only, PSRAM, ~7 KB each).
static HomeState *s_home_p, *s_next_p;
#define s_home (*s_home_p)
static uint32_t s_home_ver = 0, s_net_ver = UINT32_MAX;
static int s_room = 0;
static bool s_mqtt_ok = false;

static lv_obj_t *s_title, *s_index, *s_prev, *s_next, *s_list;
static SwitchView s_views[HOME_MAX_PANELS * HOME_SWITCHES];
static int s_view_count = 0;

// ---------------- helpers ----------------

static HomeRoom *cur_room() {
    return (s_home.have_config && s_room < s_home.cfg.room_count) ? &s_home.cfg.rooms[s_room] : nullptr;
}

static void views_apply_all() {
    HomeRoom *room = cur_room();
    if (!room) return;
    for (int i = 0; i < s_view_count; i++) {
        SwitchView &v = s_views[i];
        switch_tile_apply(v.tile, room->panels[v.panel].switches[v.sw], s_mqtt_ok);
    }
}

// Same rooms/panels/switches (ignoring on/pending)? Then only the tiles change.
static bool same_layout(const HomeConfig &a, const HomeConfig &b) {
    if (a.room_count != b.room_count) return false;
    for (int r = 0; r < a.room_count; r++) {
        const HomeRoom &ra = a.rooms[r], &rb = b.rooms[r];
        if (strcmp(ra.id, rb.id) || strcmp(ra.name, rb.name) || ra.panel_count != rb.panel_count) return false;
        for (int p = 0; p < ra.panel_count; p++) {
            const HomePanel &pa = ra.panels[p], &pb = rb.panels[p];
            if (strcmp(pa.id, pb.id) || strcmp(pa.name, pb.name)) return false;
            for (int s = 0; s < HOME_SWITCHES; s++) {
                const HomeSwitch &sa = pa.switches[s], &sb = pb.switches[s];
                if (sa.present != sb.present || sa.id != sb.id || sa.icon != sb.icon || strcmp(sa.name, sb.name))
                    return false;
            }
        }
    }
    return true;
}

static void switch_clicked_cb(lv_event_t *e) {
    SwitchView &v = s_views[(uintptr_t)lv_event_get_user_data(e)];
    HomeRoom *room = cur_room();
    if (!room || !s_mqtt_ok) return;
    const HomePanel &panel = room->panels[v.panel];
    const HomeSwitch &sw = panel.switches[v.sw];
    if (!sw.present || sw.pending) return;
    // home_service marks it pending; the dimmed tile shows on the next poll.
    home_service_set(room->id, panel.id, sw.id, !sw.on);
}

// ---------------- building ----------------

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
    // s_views holds HOME_MAX_PANELS * HOME_SWITCHES, the most a room can have.
    for (uint8_t i = 0; i < HOME_SWITCHES; i++) {
        SwitchView &v = s_views[s_view_count];
        if (switch_tile_create(v.tile, row, panel.switches[i], switch_clicked_cb, (void *)(uintptr_t)s_view_count)) {
            v.panel = panel_idx;
            v.sw = i;
            s_view_count++;
        }
    }
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
    const int n = s_home.have_config ? s_home.cfg.room_count : 0;
    nav_enable(s_prev, n > 1);
    nav_enable(s_next, n > 1);

    if (!room) {
        lv_label_set_text(s_title, s_home.have_config ? "No rooms" : "Home");
        lv_label_set_text(s_index, "");
        ui_label(s_list, FONT_S, COL_MUTED,
                 s_home.have_config ? "home/config has no rooms" : "Waiting for " MQTT_TOPIC_HOME_CONFIG "...");
        return;
    }
    lv_label_set_text(s_title, room->name);
    lv_label_set_text_fmt(s_index, "%d/%d", s_room + 1, n);
    if (!room->panel_count) ui_label(s_list, FONT_S, COL_MUTED, "No panels in this room");
    for (uint8_t p = 0; p < room->panel_count; p++) cluster_create(p, room->panels[p]);
    views_apply_all();
}

static void nav_cb(lv_event_t *e) {
    if (!s_home.have_config || s_home.cfg.room_count < 2) return;
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    s_room = (s_room + dir + s_home.cfg.room_count) % s_home.cfg.room_count;
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

// ---------------- public ----------------

void screen_home_create(lv_obj_t *tile) {
    s_home_p = static_cast<HomeState *>(state_alloc_large(sizeof(HomeState)));
    s_next_p = static_cast<HomeState *>(state_alloc_large(sizeof(HomeState)));
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
}

void screen_home_poll() {
    bool apply = false;
    if (state_version(StateGroup::Net) != s_net_ver) {
        s_net_ver = state_version(StateGroup::Net);
        const bool ok = state_get_net().mqtt_connected;
        apply = ok != s_mqtt_ok;
        s_mqtt_ok = ok;
    }
    if (state_version(StateGroup::Home) != s_home_ver) {
        s_home_ver = state_version(StateGroup::Home);
        HomeState &next = *s_next_p;
        state_get_home(next);
        if (next.have_config == s_home.have_config && same_layout(next.cfg, s_home.cfg)) {
            s_home = next;   // only switch states / pending changed
            apply = true;
        } else {
            // New layout: stay on the same room if it still exists.
            char keep_id[HOME_ID_LEN] = "";
            if (HomeRoom *r = cur_room()) strcpy(keep_id, r->id);
            s_home = next;
            s_room = 0;
            for (int i = 0; i < s_home.cfg.room_count; i++)
                if (!strcmp(s_home.cfg.rooms[i].id, keep_id)) s_room = i;
            room_build();
            apply = false;
        }
    }
    if (apply) views_apply_all();
}
