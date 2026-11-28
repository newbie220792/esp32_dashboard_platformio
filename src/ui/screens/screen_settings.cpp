#include "ui/screens/screen_settings.h"

#include <stdint.h>

#include "core/state.h"
#include "services/settings_service.h"
#include "ui/components/icon_draw.h"
#include "ui/components/overlay.h"
#include "ui/theme.h"

#define ROW_H     36   // 5 rows x 36 + 4 x 6 fit the 218px content area without scrolling
#define ROW_GAP   6
#define LABEL_W   96
#define SEG_H     32
#define SEG_MIN_W 52
#define DEG "\xC2\xB0"

struct Segment {
    lv_obj_t *btn;
    uint16_t value;
};

static Overlay s_o;
static lv_obj_t *s_slider, *s_pct, *s_city;
static Segment s_sleep[4], s_unit[2];
static uint32_t s_settings_ver = UINT32_MAX, s_loc_ver = UINT32_MAX;

// One 36px card row with a 96px bold label on the left; returns the row.
static lv_obj_t *settings_row(const char *label) {
    lv_obj_t *r = ui_card(s_o.content, 0);
    lv_obj_set_size(r, LV_PCT(100), ROW_H);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, 12, 0);
    lv_obj_t *l = ui_label(r, FONT_M_BOLD, COL_TEXT, label);
    lv_obj_set_width(l, LABEL_W);
    return r;
}

// ---------------- brightness ----------------

static void slider_cb(lv_event_t *e) {
    const int32_t v = lv_slider_get_value(s_slider);
    const bool released = lv_event_get_code(e) == LV_EVENT_RELEASED;
    lv_label_set_text_fmt(s_pct, "%ld%%", (long)v);
    settings_service_set_brightness((uint8_t)v, released);   // PWM now, NVS on release
}

// Dim end: a small muted ring. Bright end: a sun with rays.
static void sun_draw_cb(lv_event_t *e) {
    IconCtx c = icon_ctx(e);
    const bool big = lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e)) != nullptr;
    const lv_color_t col = big ? COL_TEXT : COL_MUTED;
    icon_arc(c, 50, 50, big ? 17 : 22, big ? 9 : 12, 0, 360, col);
    if (!big) return;
    static const int8_t rays[8][4] = {{50, 4, 50, 14},  {50, 86, 50, 96}, {4, 50, 14, 50},  {86, 50, 96, 50},
                                      {18, 18, 25, 25}, {75, 75, 82, 82}, {18, 82, 25, 75}, {75, 25, 82, 18}};
    for (const auto &r : rays) icon_line(c, r[0], r[1], r[2], r[3], 9, col);
}

static void sun_icon(lv_obj_t *parent, int32_t size, bool big) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, size, size);
    lv_obj_set_user_data(o, big ? (void *)1 : nullptr);
    lv_obj_add_event_cb(o, sun_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
}

static void brightness_row() {
    lv_obj_t *r = settings_row("Brightness");
    lv_obj_set_style_pad_column(r, 10, 0);
    sun_icon(r, 16, false);

    s_slider = lv_slider_create(r);
    lv_obj_set_flex_grow(s_slider, 1);
    lv_obj_set_height(s_slider, 6);
    lv_slider_set_range(s_slider, BRIGHTNESS_MIN, 100);
    lv_obj_set_style_bg_color(s_slider, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, COL_CPU, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 9, LV_PART_KNOB);   // 6 + 2*9 = 24px knob
    lv_obj_set_style_shadow_width(s_slider, 0, LV_PART_KNOB);
    lv_obj_set_ext_click_area(s_slider, 12);                // 30px tall touch strip
    lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_RELEASED, nullptr);

    sun_icon(r, 20, true);
    s_pct = ui_label(r, FONT_S_BOLD, COL_MUTED, "--%");
    lv_obj_set_width(s_pct, 36);
    lv_obj_set_style_text_align(s_pct, LV_TEXT_ALIGN_RIGHT, 0);
}

// ---------------- segmented choices ----------------

static void seg_style(lv_obj_t *b, bool selected) {
    lv_obj_set_style_bg_color(b, selected ? COL_CPU : COL_SW_OFF, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), selected ? COL_ON_ACCENT : COL_TEXT, 0);
}

static lv_obj_t *seg_group(lv_obj_t *row) {
    lv_obj_t *g = ui_box(row);
    lv_obj_set_size(g, LV_SIZE_CONTENT, SEG_H);
    lv_obj_set_flex_grow(g, 1);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g, 4, 0);
    return g;
}

static lv_obj_t *seg_button(lv_obj_t *group, const char *text, lv_event_cb_t cb, uint16_t value) {
    lv_obj_t *b = ui_box(group);
    lv_obj_set_size(b, LV_SIZE_CONTENT, SEG_H);
    lv_obj_set_style_min_width(b, SEG_MIN_W, 0);
    lv_obj_set_style_pad_hor(b, 10, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 4);
    lv_obj_t *l = ui_label(b, FONT_S_BOLD, COL_TEXT, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(uintptr_t)value);
    seg_style(b, false);
    return b;
}

static void sleep_cb(lv_event_t *e) { settings_service_set_sleep((uint16_t)(uintptr_t)lv_event_get_user_data(e)); }
static void unit_cb(lv_event_t *e) {
    settings_service_set_unit((uintptr_t)lv_event_get_user_data(e) ? TempUnit::F : TempUnit::C);
}

static void sleep_row() {
    lv_obj_t *g = seg_group(settings_row("Screen off"));
    const struct { const char *t; uint16_t s; } opts[] = {{"15s", 15}, {"30s", 30}, {"1m", 60}, {"Never", 0}};
    for (int i = 0; i < 4; i++) s_sleep[i] = {seg_button(g, opts[i].t, sleep_cb, opts[i].s), opts[i].s};
}

static void unit_row() {
    lv_obj_t *g = seg_group(settings_row("Temperature"));
    s_unit[0] = {seg_button(g, DEG "C", unit_cb, 0), 0};
    s_unit[1] = {seg_button(g, DEG "F", unit_cb, 1), 1};
}

// ---------------- location (read-only) ----------------

static void lock_draw_cb(lv_event_t *e) {
    IconCtx c = icon_ctx(e);
    icon_arc(c, 50, 42, 22, 9, 180, 360, COL_MUTED);   // shackle
    icon_line(c, 28, 42, 28, 50, 9, COL_MUTED);
    icon_line(c, 72, 42, 72, 50, 9, COL_MUTED);
    icon_rrect(c, 20, 46, 80, 86, 9, COL_MUTED);      // body
    icon_circle(c, 50, 64, 6, c.bg);                   // keyhole
}

static void location_row() {
    lv_obj_t *r = settings_row("Location");
    lv_obj_add_state(r, LV_STATE_DISABLED);   // read-only: not clickable, no arrow
    lv_obj_t *g = ui_box(r);
    lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(g, 1);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g, 8, 0);
    ui_label(g, FONT_S, COL_MUTED, "from Pi 5");
    s_city = ui_label(g, FONT_M_BOLD, COL_MUTED, "");
    lv_label_set_long_mode(s_city, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s_city, 170, 0);
    lv_obj_t *lock = ui_box(g);
    lv_obj_set_size(lock, 18, 18);
    lv_obj_add_event_cb(lock, lock_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
}

// ---------------- System info (navigation row) ----------------

// 18px chevron, #E6EDF3.
static void chevron_draw_cb(lv_event_t *e) {
    IconCtx c = icon_ctx(e);
    icon_line(c, 36, 18, 66, 50, 12, COL_TEXT);
    icon_line(c, 66, 50, 36, 82, 12, COL_TEXT);
}

static void sysinfo_cb(lv_event_t *e) {
    auto fn = reinterpret_cast<void (*)()>(lv_event_get_user_data(e));
    if (fn) fn();
}

// Navigation row: the whole row is the touch target. The subtitle is fixed text.
static void sysinfo_row(void (*on_open)()) {
    lv_obj_t *r = settings_row("System info");
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, COL_SW_OFF, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r, sysinfo_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(on_open));
    lv_obj_t *sub = ui_label(r, FONT_S, COL_MUTED, "Network, memory, firmware");
    lv_obj_set_flex_grow(sub, 1);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *arrow = ui_box(r);
    lv_obj_set_size(arrow, 18, 18);
    lv_obj_add_event_cb(arrow, chevron_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
}

// ---------------- public ----------------

lv_obj_t *screen_settings_create(void (*on_back)(), void (*on_sysinfo)()) {
    overlay_create(s_o, "Settings", on_back);
    lv_obj_set_style_pad_row(s_o.content, ROW_GAP, 0);
    brightness_row();
    sleep_row();
    unit_row();
    location_row();
    sysinfo_row(on_sysinfo);
    screen_settings_poll();
    return s_o.scr;
}

void screen_settings_poll() {
    if (state_version(StateGroup::Settings) != s_settings_ver) {
        s_settings_ver = state_version(StateGroup::Settings);
        const Settings s = state_get_settings();
        if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {   // don't fight the finger
            lv_slider_set_value(s_slider, s.brightness, LV_ANIM_OFF);
            lv_label_set_text_fmt(s_pct, "%u%%", (unsigned)s.brightness);
        }
        for (const Segment &sg : s_sleep) seg_style(sg.btn, sg.value == s.sleep_s);
        for (const Segment &sg : s_unit) seg_style(sg.btn, sg.value == (s.unit == TempUnit::F ? 1 : 0));
    }
    if (state_version(StateGroup::Location) != s_loc_ver) {
        s_loc_ver = state_version(StateGroup::Location);
        lv_label_set_text(s_city, state_get_location().loc.city);
    }
}
