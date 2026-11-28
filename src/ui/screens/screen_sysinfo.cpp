#include "ui/screens/screen_sysinfo.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/sysinfo_service.h"
#include "ui/components/overlay.h"
#include "ui/theme.h"

#define CARD_TITLE_H 18
#define TEXT_ROW_H   26
#define BAR_ROW_H    36
#define BAR_H        6
#define DOT          8
#define SSID_MAX_W   140
#define BAR_RANGE    1000

#define COL_YELLOW  COL_TEMP       // #F5A524
#define COL_RED     COL_STOPPED    // #FF6B6B
#define COL_BAR_OFF COL_PILL_OFF   // #3A4656: unlit signal bar
#define BULLET      "\xE2\x80\xA2"

// Label whose text lives in `buf` (lv_label_set_text_static): no LVGL
// allocation on refresh, and only redrawn when the text really changes.
struct Text {
    lv_obj_t *l;
    char buf[28];
};

// Object with a cached colour (bg or bar indicator), restyled only on change.
struct Paint {
    lv_obj_t *o;
    lv_color_t c;
    bool valid;
};

struct Meter {   // one 36px row: label, value, 6px bar
    Text  value;
    lv_obj_t *bar;
    Paint fill;
    int32_t pos;
};

static Overlay s_o;
static lv_obj_t *s_scr;
static lv_timer_t *s_timer;
static uint32_t s_net_ver, s_api_ver;
static int s_opens;

// network card
static lv_obj_t *s_ssid;
static char s_ssid_text[sizeof(SysInfo::ssid)];
static Text s_ip, s_rssi, s_mqtt, s_api, s_time;
static Paint s_bars[4], s_mqtt_dot, s_api_dot, s_time_dot;
// device card
static Text s_fw, s_uptime;
static Meter s_heap, s_psram, s_flash;

// ---------------- cached setters ----------------

static void text_set(Text &t, const char *s) {
    if (strncmp(t.buf, s, sizeof t.buf - 1) == 0) return;
    strncpy(t.buf, s, sizeof t.buf - 1);
    t.buf[sizeof t.buf - 1] = '\0';
    lv_label_set_text_static(t.l, t.buf);   // same pointer: still refreshes
}

static void paint_set(Paint &p, lv_color_t c, lv_style_selector_t part = LV_PART_MAIN) {
    if (p.valid && lv_color_eq(p.c, c)) return;
    p.c = c;
    p.valid = true;
    lv_obj_set_style_bg_color(p.o, c, part);
}

static void meter_set(Meter &m, const char *text, int32_t permille, lv_color_t c) {
    text_set(m.value, text);
    if (permille < 0) permille = 0;
    if (permille > BAR_RANGE) permille = BAR_RANGE;
    if (permille != m.pos) {
        m.pos = permille;
        lv_bar_set_value(m.bar, permille, LV_ANIM_OFF);
    }
    paint_set(m.fill, c, LV_PART_INDICATOR);
}

// ---------------- widgets ----------------

static lv_obj_t *card(lv_obj_t *parent, const char *title) {
    lv_obj_t *c = ui_card(parent, 0);
    lv_obj_set_height(c, LV_PCT(100));
    lv_obj_set_flex_grow(c, 1);   // two equal columns
    lv_obj_set_style_pad_ver(c, 8, 0);
    lv_obj_set_style_pad_hor(c, 12, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *t = ui_label(c, FONT_S_BOLD, COL_CPU, title);
    lv_obj_set_height(t, CARD_TITLE_H);
    return c;
}

static lv_obj_t *row(lv_obj_t *card, int32_t h, const char *label) {
    lv_obj_t *r = ui_box(card);
    lv_obj_set_size(r, LV_PCT(100), h);
    lv_obj_t *l = ui_label(r, FONT_S, COL_MUTED, label);
    lv_obj_align(l, h == TEXT_ROW_H ? LV_ALIGN_LEFT_MID : LV_ALIGN_TOP_LEFT, 0, h == TEXT_ROW_H ? 0 : 3);
    return r;
}

// Right side of a text row: [indicator] value, vertically centred.
static lv_obj_t *value_group(lv_obj_t *r) {
    lv_obj_t *g = ui_box(r);
    lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g, 6, 0);
    lv_obj_align(g, LV_ALIGN_RIGHT_MID, 0, 0);
    return g;
}

static void value_label(lv_obj_t *parent, Text &t) {
    strcpy(t.buf, "--");
    t.l = lv_label_create(parent);
    lv_obj_set_style_text_font(t.l, FONT_S_BOLD, 0);
    lv_obj_set_style_text_color(t.l, COL_TEXT, 0);
    lv_label_set_text_static(t.l, t.buf);
}

static void dot(lv_obj_t *parent, Paint &p) {
    p.o = ui_box(parent);
    lv_obj_set_size(p.o, DOT, DOT);
    lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(p.o, LV_OPA_COVER, 0);
    p.valid = false;
    paint_set(p, COL_GRAY);
}

static void text_row(lv_obj_t *card, const char *label, Text &t, Paint *indicator = nullptr) {
    lv_obj_t *g = value_group(row(card, TEXT_ROW_H, label));
    if (indicator) dot(g, *indicator);
    value_label(g, t);
}

// 4 bars, 3px wide, 2px apart, 5/8/11/14px tall, bottom-aligned.
static void signal_bars(lv_obj_t *parent) {
    lv_obj_t *box = ui_box(parent);
    lv_obj_set_size(box, 4 * 3 + 3 * 2, 14);
    for (int i = 0; i < 4; i++) {
        const int32_t h = 5 + 3 * i;
        Paint &p = s_bars[i];
        p.o = ui_box(box);
        lv_obj_set_size(p.o, 3, h);
        lv_obj_set_pos(p.o, i * 5, 14 - h);
        lv_obj_set_style_radius(p.o, 1, 0);
        lv_obj_set_style_bg_opa(p.o, LV_OPA_COVER, 0);
        p.valid = false;
        paint_set(p, COL_BAR_OFF);
    }
}

static void meter_row(lv_obj_t *card, const char *label, Meter &m) {
    lv_obj_t *r = row(card, BAR_ROW_H, label);
    value_label(r, m.value);
    lv_obj_align(m.value.l, LV_ALIGN_TOP_RIGHT, 0, 3);

    m.bar = lv_bar_create(r);
    lv_obj_remove_style_all(m.bar);
    lv_obj_set_size(m.bar, LV_PCT(100), BAR_H);
    lv_obj_align(m.bar, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_bar_set_range(m.bar, 0, BAR_RANGE);
    lv_obj_set_style_bg_color(m.bar, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(m.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(m.bar, 3, LV_PART_INDICATOR);
    m.fill = {m.bar, COL_TRACK, false};
    m.pos = 0;
    paint_set(m.fill, COL_TRACK, LV_PART_INDICATOR);
}

// ---------------- refresh ----------------

// Free-space bars: shorter = worse. Yellow under 20 % free, red under 10 %.
static lv_color_t free_color(int32_t permille, lv_color_t normal) {
    return permille < 100 ? COL_RED : permille < 200 ? COL_YELLOW : normal;
}

static int32_t permille(uint64_t part, uint64_t total) { return total ? (int32_t)(part * BAR_RANGE / total) : 0; }

static void refresh() {
    SysInfo si;
    sysinfo_service_read(si);
    char b[32];

    // ---- network ----
    const char *ssid = si.wifi_connected && si.ssid[0] ? si.ssid : "--";
    if (strcmp(s_ssid_text, ssid) != 0) {   // dynamic text: LONG_DOT edits the label's own copy
        strncpy(s_ssid_text, ssid, sizeof s_ssid_text - 1);
        lv_label_set_text(s_ssid, s_ssid_text);
    }

    if (si.wifi_connected && si.ip) {
        snprintf(b, sizeof b, "%u.%u.%u.%u", (unsigned)(si.ip & 0xFF), (unsigned)((si.ip >> 8) & 0xFF),
                 (unsigned)((si.ip >> 16) & 0xFF), (unsigned)(si.ip >> 24));
        text_set(s_ip, b);
    } else {
        text_set(s_ip, "--");
    }

    const int bars = si.wifi_connected ? rssi_bars(si.rssi) : 0;
    const lv_color_t lit = bars >= 3 ? COL_OK : bars == 2 ? COL_YELLOW : COL_RED;
    for (int i = 0; i < 4; i++) paint_set(s_bars[i], i < bars ? lit : COL_BAR_OFF);
    if (si.wifi_connected && si.rssi < 0) {
        snprintf(b, sizeof b, "%d dBm", (int)si.rssi);
        text_set(s_rssi, b);
    } else {
        text_set(s_rssi, "--");
    }

    paint_set(s_mqtt_dot, si.mqtt_connected ? COL_OK : COL_RED);
    text_set(s_mqtt, si.mqtt_connected ? "Connected" : "Offline");

    switch (si.api) {
    case ApiHealth::Ok:
        paint_set(s_api_dot, COL_OK);
        snprintf(b, sizeof b, "OK " BULLET " %u ms", (unsigned)si.api_rtt_ms);
        text_set(s_api, b);
        break;
    case ApiHealth::Error:
        paint_set(s_api_dot, COL_RED);
        text_set(s_api, "Error");
        break;
    case ApiHealth::NotConfigured:
        paint_set(s_api_dot, COL_GRAY);
        text_set(s_api, "Not set");
        break;
    case ApiHealth::Unknown:
    default:
        paint_set(s_api_dot, COL_GRAY);
        text_set(s_api, "--");
        break;
    }

    if (si.time_synced) {
        const time_t t = si.last_sync ? (time_t)si.last_sync : time(nullptr);
        struct tm tm;
        localtime_r(&t, &tm);
        snprintf(b, sizeof b, "Synced %02d:%02d", tm.tm_hour, tm.tm_min);
        paint_set(s_time_dot, COL_OK);
        text_set(s_time, b);
    } else {
        paint_set(s_time_dot, COL_RED);
        text_set(s_time, "Not synced");
    }

    // ---- device ----
    snprintf(b, sizeof b, "v%s", si.fw_version);
    text_set(s_fw, b);
    format_uptime(b, sizeof b, si.uptime_s);
    text_set(s_uptime, b);

    int32_t pm = permille(si.heap_free, si.heap_total);
    format_size_pair(b, sizeof b, si.heap_free, si.heap_total);
    meter_set(s_heap, si.heap_total ? b : "--", pm, free_color(pm, COL_CPU));

    if (si.psram_total) {
        pm = permille(si.psram_free, si.psram_total);
        format_size_pair(b, sizeof b, si.psram_free, si.psram_total);
        meter_set(s_psram, b, pm, free_color(pm, COL_RAM));
    } else {
        meter_set(s_psram, "--", 0, COL_TRACK);
    }

    // Used space against the single app partition (huge_app.csv, ~3 MB, no OTA):
    // longer = closer to the limit.
    if (si.sketch_size && si.app_size) {
        pm = permille(si.sketch_size, si.app_size);
        format_size_pair(b, sizeof b, si.sketch_size, si.app_size);
        meter_set(s_flash, b, pm, pm >= 950 ? COL_RED : pm >= 800 ? COL_YELLOW : COL_CPU);
    } else {
        meter_set(s_flash, "--", 0, COL_TRACK);
    }
}

static void timer_cb(lv_timer_t *) { refresh(); }

// ---------------- lifecycle ----------------

static void log_mem(const char *when) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    Serial.printf("[ui] sysinfo #%d %s: LVGL free %u (used %u%%), internal heap %u\n", s_opens, when,
                  (unsigned)m.free_size, m.used_pct, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static void after_delete_cb(void *) { log_mem("after close"); }

static void delete_cb(lv_event_t *) {
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = nullptr;
    s_scr = nullptr;
    s_ssid = nullptr;
    sysinfo_service_set_open(false);
    lv_async_call(after_delete_cb, nullptr);   // once the children are gone too
}

lv_obj_t *screen_sysinfo_create(void (*on_back)()) {
    if (s_scr) return s_scr;
    s_opens++;
    log_mem("before open");

    overlay_create(s_o, "System info", on_back);
    s_scr = s_o.scr;
    lv_obj_add_event_cb(s_scr, delete_cb, LV_EVENT_DELETE, nullptr);

    lv_obj_set_flex_flow(s_o.content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_o.content, UI_GAP, 0);

    lv_obj_t *net = card(s_o.content, "NETWORK");
    lv_obj_t *g = value_group(row(net, TEXT_ROW_H, "Wi-Fi"));
    s_ssid = ui_label(g, FONT_S_BOLD, COL_TEXT, "--");
    lv_label_set_long_mode(s_ssid, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s_ssid, SSID_MAX_W, 0);
    strcpy(s_ssid_text, "--");
    text_row(net, "IP", s_ip);
    g = value_group(row(net, TEXT_ROW_H, "Signal"));
    signal_bars(g);
    value_label(g, s_rssi);
    text_row(net, "MQTT", s_mqtt, &s_mqtt_dot);
    text_row(net, "Pi 5 API", s_api, &s_api_dot);
    text_row(net, "Time", s_time, &s_time_dot);

    lv_obj_t *dev = card(s_o.content, "DEVICE");
    text_row(dev, "Firmware", s_fw);
    text_row(dev, "Uptime", s_uptime);
    meter_row(dev, "Heap free", s_heap);
    meter_row(dev, "PSRAM free", s_psram);
    meter_row(dev, "Firmware size", s_flash);

    s_net_ver = state_version(StateGroup::Net);
    s_api_ver = state_version(StateGroup::ApiHealth);
    sysinfo_service_set_open(true);
    refresh();
    s_timer = lv_timer_create(timer_cb, SYSINFO_REFRESH_MS, nullptr);
    log_mem("built");
    return s_scr;
}

lv_obj_t *screen_sysinfo_get() { return s_scr; }

void screen_sysinfo_drop() {
    if (s_scr && s_scr != lv_screen_active()) lv_obj_delete(s_scr);   // delete_cb clears s_scr
}

void screen_sysinfo_poll() {
    if (!s_scr) return;
    const uint32_t nv = state_version(StateGroup::Net), av = state_version(StateGroup::ApiHealth);
    if (nv == s_net_ver && av == s_api_ver) return;
    s_net_ver = nv;
    s_api_ver = av;
    refresh();   // Wi-Fi / MQTT / Pi 5 changes show at once, not on the next tick
}
