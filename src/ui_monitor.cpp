#include "ui_monitor.h"

#include <stdio.h>

#include "app_config.h"
#include "ui_theme.h"

#define TEMP_MAX_C     85
#define TEMP_HOT_C     70
#define GAUGE_ROW_H    80
#define ARC_SIZE       56
#define ARC_WIDTH      6
#define SVC_ROW_H      20
#define SVC_STATUS_W   48
#define SVC_PORT_W     44
#define STALE_CHECK_MS 1000

struct Gauge {
    lv_obj_t *arc;
    lv_obj_t *value;
    lv_obj_t *caption;
};

struct ServiceRow {
    lv_obj_t *row, *name, *dot, *port;
};

static Gauge g_cpu, g_ram, g_temp;
static lv_obj_t *s_svc_list;
static lv_obj_t *s_svc_empty;
static ServiceRow s_rows[MAX_SERVICES];
static uint32_t s_last_rx_ms = 0;
static bool s_have_data = false;

// ---------------- gauges ----------------

static void gauge_create(Gauge &g, lv_obj_t *row, lv_color_t color, const char *caption) {
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

static void gauge_set(Gauge &g, int32_t arc_value, const char *text) {
    lv_arc_set_value(g.arc, arc_value);
    lv_label_set_text(g.value, text);
    lv_obj_set_style_text_color(g.value, COL_TEXT, 0);
}

// Gray out the values when pi5/stats has gone quiet.
static void stale_timer_cb(lv_timer_t *) {
    if (s_have_data && lv_tick_elaps(s_last_rx_ms) >= MQTT_STALE_MS) {
        lv_obj_set_style_text_color(g_cpu.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_ram.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_temp.value, COL_MUTED, 0);
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
    lv_obj_set_size(h, LV_PCT(100), 14);
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

    r.name = ui_label(r.row, FONT_S_BOLD, COL_TEXT, "");
    lv_obj_set_flex_grow(r.name, 1);
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);

    lv_obj_t *st = cell(r.row, SVC_STATUS_W);
    r.dot = ui_box(st);
    lv_obj_set_size(r.dot, 8, 8);
    lv_obj_set_style_radius(r.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(r.dot);

    lv_obj_t *pc = cell(r.row, SVC_PORT_W);
    r.port = ui_label(pc, FONT_S, COL_MUTED, "-");
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
    lv_obj_set_style_pad_hor(card, 10, 0);
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

void ui_monitor_create(lv_obj_t *tile) {
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

void ui_monitor_update(const PiStats &s) {
    char buf[24];
    s_last_rx_ms = lv_tick_get();
    s_have_data = true;

    snprintf(buf, sizeof buf, "%d%%", (int)(s.cpu + 0.5f));
    gauge_set(g_cpu, (int32_t)(s.cpu + 0.5f), buf);

    snprintf(buf, sizeof buf, "%d%%", (int)(s.ram + 0.5f));
    gauge_set(g_ram, (int32_t)(s.ram + 0.5f), buf);
    if (s.ram_total > 0)
        lv_label_set_text_fmt(g_ram.caption, "RAM %.1f/%gG", s.ram_used, s.ram_total);

    snprintf(buf, sizeof buf, "%d\xC2\xB0", (int)(s.temp + 0.5f));   // degree sign
    gauge_set(g_temp, (int32_t)(s.temp + 0.5f), buf);
    lv_obj_set_style_arc_color(g_temp.arc, s.temp > TEMP_HOT_C ? COL_HOT : COL_TEMP, LV_PART_INDICATOR);
}

void ui_monitor_set_services(const ServiceList &list) {
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
