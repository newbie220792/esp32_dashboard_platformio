#include "ui/screens/screen_monitor.h"

#include <math.h>
#include <stdio.h>

#include "app_config.h"
#include "core/state.h"
#include "ui/components/gauge.h"
#include "ui/theme.h"

#define TEMP_MAX_C     85
#define TEMP_HOT_C     70
#define GAUGE_ROW_H    84
#define SVC_HEADER_H   16
#define SVC_ROW_H      24     // 4 rows visible, more scroll
#define SVC_STATUS_W   72
#define SVC_PORT_W     72
#define STALE_CHECK_MS 1000

struct ServiceRow {
    lv_obj_t *row, *name, *dot, *port;
};

static Gauge g_cpu, g_ram, g_temp;
static lv_obj_t *s_svc_list;
static lv_obj_t *s_svc_empty;
static ServiceRow s_rows[MAX_SERVICES];
static uint32_t s_stats_ver = UINT32_MAX, s_services_ver = UINT32_MAX, s_settings_ver = UINT32_MAX;
static Pi5StatsState s_pi5;   // last copy drawn
static TempUnit s_unit = TempUnit::C;   // TEMP label only; arc + hot threshold stay deg C

// Gray out the values when pi5/stats has gone quiet.
static void stale_timer_cb(lv_timer_t *) {
    if (s_pi5.have_data && lv_tick_elaps(s_pi5.rx_ms) >= MQTT_STALE_MS) {
        gauge_set_stale(g_cpu);
        gauge_set_stale(g_ram);
        gauge_set_stale(g_temp);
    }
}

// ---------------- service list ----------------

// Fixed-width cell; children laid out by the caller.
static lv_obj_t *cell(lv_obj_t *row, int32_t w) {
    lv_obj_t *c = ui_box(row);
    lv_obj_set_size(c, w, LV_PCT(100));
    return c;
}

static void header_row(lv_obj_t *card) {
    lv_obj_t *h = ui_box(card);
    lv_obj_set_size(h, LV_PCT(100), SVC_HEADER_H);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *n = ui_label(h, FONT_S, COL_MUTED, "Service");
    lv_obj_set_flex_grow(n, 1);
    lv_obj_t *st = ui_label(cell(h, SVC_STATUS_W), FONT_S, COL_MUTED, "Status");
    lv_obj_center(st);
    lv_obj_t *pt = ui_label(cell(h, SVC_PORT_W), FONT_S, COL_MUTED, "Port");
    lv_obj_align(pt, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void service_row_create(ServiceRow &r) {
    r.row = ui_box(s_svc_list);
    lv_obj_set_size(r.row, LV_PCT(100), SVC_ROW_H);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    r.name = ui_label(r.row, FONT_M, COL_TEXT, "");
    lv_obj_set_flex_grow(r.name, 1);
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);

    lv_obj_t *st = cell(r.row, SVC_STATUS_W);
    r.dot = ui_box(st);
    lv_obj_set_size(r.dot, 8, 8);
    lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(r.dot);

    lv_obj_t *pc = cell(r.row, SVC_PORT_W);
    r.port = ui_label(pc, FONT_M, COL_MUTED, "-");
    lv_obj_align(r.port, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
}

// running = filled green dot; stopped = hollow red ring (shape differs too,
// so it reads without relying on colour).
static void service_dot_set(lv_obj_t *dot, bool running) {
    if (running) {
        lv_obj_set_style_bg_color(dot, COL_OK, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
    } else {
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(dot, COL_STOPPED, 0);
        lv_obj_set_style_border_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 2, 0);
    }
}

static void services_card_create(lv_obj_t *tile) {
    lv_obj_t *card = ui_card(tile, 6);
    lv_obj_set_style_pad_hor(card, 16, 0);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 2, 0);

    header_row(card);

    s_svc_list = ui_box(card);
    lv_obj_set_width(s_svc_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_svc_list, 1);
    lv_obj_set_flex_flow(s_svc_list, LV_FLEX_FLOW_COLUMN);
    // Must be clickable: LVGL picks the scroll target by walking up from the
    // pressed object, and the (non-clickable) rows fall through to this list.
    // Horizontal drags chain up to the tileview, so swiping still works.
    lv_obj_add_flag(s_svc_list, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_svc_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_svc_list, LV_DIR_VER);
    lv_obj_set_style_pad_right(s_svc_list, 6, 0);   // keep the port column clear of the scrollbar
    ui_style_scrollbar(s_svc_list);

    for (auto &r : s_rows) service_row_create(r);
    s_svc_empty = ui_label(s_svc_list, FONT_S, COL_MUTED, "Waiting for " MQTT_TOPIC_SERVICES "...");
}

// ---------------- public ----------------

void screen_monitor_create(lv_obj_t *tile) {
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, UI_GAP, 0);

    lv_obj_t *row = ui_box(tile);
    lv_obj_set_size(row, LV_PCT(100), GAUGE_ROW_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    gauge_create(g_cpu, row, COL_CPU, "CPU");
    gauge_create(g_ram, row, COL_RAM, "RAM");
    gauge_create(g_temp, row, COL_TEMP, "TEMP");
    lv_arc_set_range(g_temp.arc, 0, TEMP_MAX_C);

    services_card_create(tile);

    lv_timer_create(stale_timer_cb, STALE_CHECK_MS, nullptr);
}

static void stats_draw(const PiStats &s) {
    char buf[24];

    snprintf(buf, sizeof buf, "%d%%", (int)(s.cpu + 0.5f));
    gauge_set(g_cpu, (int32_t)(s.cpu + 0.5f), buf);

    snprintf(buf, sizeof buf, "%d%%", (int)(s.ram + 0.5f));
    gauge_set(g_ram, (int32_t)(s.ram + 0.5f), buf);
    if (s.ram_total > 0)
        lv_label_set_text_fmt(g_ram.caption, "RAM %.1f/%gG", s.ram_used, s.ram_total);

    snprintf(buf, sizeof buf, "%d\xC2\xB0", (int)lroundf(temp_in_unit(s.temp, s_unit)));   // degree sign
    gauge_set(g_temp, (int32_t)(s.temp + 0.5f), buf);
    lv_obj_set_style_arc_color(g_temp.arc, s.temp > TEMP_HOT_C ? COL_HOT : COL_TEMP, LV_PART_INDICATOR);
}

static void services_draw(const ServiceList &list) {
    for (int i = 0; i < MAX_SERVICES; i++) {
        ServiceRow &r = s_rows[i];
        if (i >= list.count) {
            lv_obj_add_flag(r.row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const ServiceInfo &it = list.items[i];
        lv_label_set_text(r.name, it.name);
        service_dot_set(r.dot, it.running);
        if (it.port >= 0) lv_label_set_text_fmt(r.port, "%ld", (long)it.port);
        else lv_label_set_text(r.port, "-");
        lv_obj_remove_flag(r.row, LV_OBJ_FLAG_HIDDEN);
    }
    if (list.count) lv_obj_add_flag(s_svc_empty, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_label_set_text(s_svc_empty, "No services reported");
        lv_obj_remove_flag(s_svc_empty, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_monitor_poll() {
    if (state_version(StateGroup::Settings) != s_settings_ver) {
        s_settings_ver = state_version(StateGroup::Settings);
        s_unit = state_get_settings().unit;
        s_stats_ver = UINT32_MAX;   // redraw the TEMP label in the new unit
    }
    if (state_version(StateGroup::Pi5Stats) != s_stats_ver) {
        s_stats_ver = state_version(StateGroup::Pi5Stats);
        state_get_pi5_stats(s_pi5);
        if (s_pi5.have_data) stats_draw(s_pi5.stats);
    }
    if (state_version(StateGroup::Services) != s_services_ver) {
        s_services_ver = state_version(StateGroup::Services);
        static ServiceList list;   // LVGL task only
        state_get_services(list);
        if (s_services_ver) services_draw(list);   // version 0 = nothing received yet
    }
}
