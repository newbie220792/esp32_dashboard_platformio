#include "ui/screens/screen_alerts.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <time.h>

#include "core/parsers.h"
#include "core/state.h"
#include "services/alerts_service.h"
#include "ui/components/alert_icon.h"
#include "ui/components/overlay.h"
#include "ui/theme.h"

#define ROW_H      46
#define ROW_GAP    6
#define ICON       28
#define BTN_H      32
#define ERR_H      36
#define AGE_MS     30000   // relative times ("2 min") refresh

struct AlertRow {
    lv_obj_t *row, *icon, *title, *detail, *age;
};

enum class ErrAction : uintptr_t { Reload, Clear };

static Overlay s_o;
static lv_obj_t *s_count, *s_clear, *s_err, *s_err_text, *s_err_btn, *s_stage, *s_list, *s_spinner, *s_empty;
static AlertRow s_rows[ALERTS_MAX];
static int s_row_count;   // rows created since the screen opened
static bool s_open;
static uint32_t s_ver = UINT32_MAX;
static AlertsState s_st;   // last copy drawn (LVGL task only)
static lv_timer_t *s_age_timer;

// ---------------- widgets ----------------

static lv_obj_t *button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, BTN_H);
    lv_obj_set_style_pad_hor(b, 12, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, COL_SW_OFF, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, 4);
    lv_obj_t *l = ui_label(b, FONT_S_BOLD, COL_TEXT, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static void button_enable(lv_obj_t *b, bool en) {
    if (en) lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(b, en ? LV_OPA_COVER : LV_OPA_50, 0);
}

static void clear_cb(lv_event_t *) {
    alerts_service_clear_all();
    button_enable(s_clear, false);   // until the answer arrives
}

static void err_cb(lv_event_t *) {
    const ErrAction a = (ErrAction)(uintptr_t)lv_obj_get_user_data(s_err_btn);
    if (a == ErrAction::Clear) alerts_service_clear_all();
    else alerts_service_refresh();
    button_enable(s_err_btn, false);
}

static void row_create(AlertRow &r) {
    r.row = ui_card(s_list, 0);
    lv_obj_set_size(r.row, LV_PCT(100), ROW_H);
    lv_obj_set_style_pad_hor(r.row, 12, 0);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r.row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r.row, 12, 0);

    r.icon = alert_icon_create(r.row, ICON);

    lv_obj_t *col = ui_box(r.row);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    r.title = ui_label(col, FONT_M_BOLD, COL_TEXT, "");
    lv_label_set_long_mode(r.title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(r.title, LV_PCT(100));
    r.detail = ui_label(col, FONT_S, COL_MUTED, "");
    lv_label_set_long_mode(r.detail, LV_LABEL_LONG_DOT);
    lv_obj_set_width(r.detail, LV_PCT(100));

    r.age = ui_label(r.row, FONT_S, COL_MUTED, "");
}

static void row_set(AlertRow &r, const Alert &a) {
    alert_icon_set(r.icon, a.level);
    if (a.count > 1) lv_label_set_text_fmt(r.title, "%s (x%u)", a.title, (unsigned)a.count);
    else lv_label_set_text(r.title, a.title);
    lv_label_set_text(r.detail, a.detail);
}

static void ages_refresh() {
    const uint32_t now = (uint32_t)time(nullptr);
    char buf[16];
    for (int i = 0; i < s_row_count && i < s_st.list.count; i++) {
        format_age(buf, sizeof buf, s_st.list.items[i].ts, now);
        lv_label_set_text(s_rows[i].age, buf);
    }
}

static void age_timer_cb(lv_timer_t *) { ages_refresh(); }

static void show(lv_obj_t *o, bool on) {
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// ---------------- draw ----------------

static void draw() {
    const AlertList &l = s_st.list;
    const bool busy = s_st.op != AlertsOp::Idle;

    // rows: create lazily (max ALERTS_MAX), update in place so the scroll
    // position survives the periodic refresh
    const int n = s_st.have_list ? (l.count < ALERTS_MAX ? l.count : ALERTS_MAX) : 0;
    while (s_row_count < n) row_create(s_rows[s_row_count++]);
    for (int i = 0; i < s_row_count; i++) {
        show(s_rows[i].row, i < n);
        if (i < n) row_set(s_rows[i], l.items[i]);
    }
    ages_refresh();

    if (s_st.have_list) lv_label_set_text_fmt(s_count, "%d alert%s", n, n == 1 ? "" : "s");
    else lv_label_set_text(s_count, "");
    button_enable(s_clear, n > 0 && !busy);

    show(s_spinner, !s_st.have_list && s_st.op == AlertsOp::Loading);   // first load only
    show(s_empty, s_st.have_list && n == 0 && s_st.op != AlertsOp::Loading && s_st.err == AlertsErr::None);

    const bool err = s_st.err != AlertsErr::None;
    show(s_err, err);
    if (err) {
        const bool clear = s_st.err == AlertsErr::ClearFailed;
        lv_label_set_text(s_err_text, clear ? "Couldn't clear alerts" : "Couldn't load alerts");
        lv_obj_set_user_data(s_err_btn, (void *)(uintptr_t)(clear ? ErrAction::Clear : ErrAction::Reload));
        button_enable(s_err_btn, !busy);
    }
}

// ---------------- public ----------------

lv_obj_t *screen_alerts_create(void (*on_back)()) {
    overlay_create(s_o, "Alerts", on_back);

    s_count = ui_label(s_o.bar, FONT_S_BOLD, COL_MUTED, "");
    s_clear = button(s_o.bar, "Clear all", clear_cb, nullptr);
    button_enable(s_clear, false);

    // error bar: message + Retry; the rows already shown stay below it
    s_err = ui_card(s_o.content, 0);
    lv_obj_set_size(s_err, LV_PCT(100), ERR_H);
    lv_obj_set_style_pad_left(s_err, 12, 0);
    lv_obj_set_style_pad_right(s_err, 4, 0);
    lv_obj_set_flex_flow(s_err, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_err, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_err, 8, 0);
    lv_obj_t *dot = ui_box(s_err);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_STOPPED, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    s_err_text = ui_label(s_err, FONT_M, COL_TEXT, "");
    lv_obj_set_flex_grow(s_err_text, 1);
    s_err_btn = button(s_err, "Retry", err_cb, nullptr);
    lv_obj_set_height(s_err_btn, 28);
    show(s_err, false);

    // stage: the scrolling list, with the spinner / "No alerts" floating over it
    s_stage = ui_box(s_o.content);
    lv_obj_set_width(s_stage, LV_PCT(100));
    lv_obj_set_flex_grow(s_stage, 1);

    s_list = ui_box(s_stage);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_list, ROW_GAP, 0);
    lv_obj_set_style_pad_right(s_list, 8, 0);   // rows clear of the 3px scrollbar
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_CLICKABLE);   // see CLAUDE.md: scroll target
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    ui_style_scrollbar(s_list);

    s_spinner = lv_spinner_create(s_stage);
    lv_obj_set_size(s_spinner, 36, 36);
    lv_spinner_set_anim_params(s_spinner, 1000, 270);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_spinner, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_spinner, COL_CPU, LV_PART_INDICATOR);
    lv_obj_center(s_spinner);
    show(s_spinner, false);

    s_empty = ui_label(s_stage, FONT_M, COL_MUTED, "No alerts");
    lv_obj_center(s_empty);
    show(s_empty, false);

    s_age_timer = lv_timer_create(age_timer_cb, AGE_MS, nullptr);
    lv_timer_pause(s_age_timer);
    return s_o.scr;
}

void screen_alerts_open() {
    s_open = true;
    s_ver = UINT32_MAX;   // draw the kept data at once
    alerts_service_set_open(true);
    lv_obj_scroll_to_y(s_list, 0, LV_ANIM_OFF);
    lv_timer_resume(s_age_timer);
    screen_alerts_poll();
}

void screen_alerts_close() {
    if (!s_open) return;
    s_open = false;
    alerts_service_set_open(false);
    lv_timer_pause(s_age_timer);
    lv_obj_clean(s_list);   // free the rows; the data stays in the state store
    s_row_count = 0;
}

void screen_alerts_poll() {
    if (!s_open) return;
    const uint32_t v = state_version(StateGroup::Alerts);
    if (v == s_ver) return;
    s_ver = v;
    state_get_alerts(s_st);
    draw();
    static bool measured = false;   // RAM with the rows built (task 8, step 8)
    if (!measured && s_row_count == ALERTS_MAX) {
        measured = true;
        lv_mem_monitor_t m;
        lv_mem_monitor(&m);
        Serial.printf("[ui] alerts: %d rows, LVGL used %u%% (free %u), internal heap %u\n", s_row_count,
                      m.used_pct, (unsigned)m.free_size,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
}
