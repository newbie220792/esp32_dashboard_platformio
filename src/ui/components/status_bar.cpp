#include "ui/components/status_bar.h"

#include <time.h>

#include "ui/theme.h"

#define MAX_PAGES 8

static lv_obj_t *s_pills[MAX_PAGES];
static int s_pages;
static lv_obj_t *s_dot, *s_title, *s_wifi, *s_clock;
static void (*s_on_wifi_tap)();
// Last values set: the setters are called on every UI poll, so skip no-ops
// (each LVGL set would invalidate and redraw the bar).
static int s_page = -1, s_dot_ok = -1, s_wifi_ok = -1;
static const char *s_title_text;

static void clock_timer_cb(lv_timer_t *) {
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    if (tmv.tm_year + 1900 < 2024) {   // NTP not synced yet
        lv_label_set_text(s_clock, "--:--");
        return;
    }
    lv_label_set_text_fmt(s_clock, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
}

static void wifi_icon_cb(lv_event_t *) {
    if (s_on_wifi_tap) s_on_wifi_tap();
}

static lv_obj_t *row(lv_obj_t *parent, lv_flex_align_t main, int32_t gap, lv_align_t align) {
    lv_obj_t *r = ui_box(parent);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, main, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, gap, 0);
    lv_obj_align(r, align, 0, 0);
    return r;
}

void status_bar_create(lv_obj_t *scr, int page_count, void (*on_wifi_tap)()) {
    s_pages = page_count < MAX_PAGES ? page_count : MAX_PAGES;
    s_on_wifi_tap = on_wifi_tap;

    lv_obj_t *bar = ui_box(scr);
    lv_obj_set_size(bar, LV_PCT(100), UI_STATUS_H);
    lv_obj_set_style_pad_hor(bar, UI_PAD, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    // left: dot + label
    lv_obj_t *left = row(bar, LV_FLEX_ALIGN_START, 8, LV_ALIGN_LEFT_MID);
    s_dot = ui_box(left);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dot, COL_GRAY, 0);
    s_title = ui_label(left, FONT_M, COL_TEXT, "");

    // center: page pills
    lv_obj_t *pills = row(bar, LV_FLEX_ALIGN_CENTER, 5, LV_ALIGN_CENTER);
    for (int i = 0; i < s_pages; i++) {
        s_pills[i] = ui_box(pills);
        lv_obj_set_size(s_pills[i], 8, 4);
        lv_obj_set_style_radius(s_pills[i], 2, 0);
        lv_obj_set_style_bg_opa(s_pills[i], LV_OPA_COVER, 0);
    }

    // right: wifi + clock
    lv_obj_t *right = row(bar, LV_FLEX_ALIGN_END, 8, LV_ALIGN_RIGHT_MID);
    s_wifi = ui_label(right, FONT_L, COL_GRAY, LV_SYMBOL_WIFI);   // ~20px glyph
    lv_obj_add_flag(s_wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_wifi, 6);
    lv_obj_add_event_cb(s_wifi, wifi_icon_cb, LV_EVENT_CLICKED, nullptr);
    s_clock = ui_label(right, FONT_M, COL_TEXT, "--:--");

    lv_timer_create(clock_timer_cb, 1000, nullptr);
    clock_timer_cb(nullptr);
}

void status_bar_set_page(int active) {
    if (active == s_page) return;
    s_page = active;
    for (int i = 0; i < s_pages; i++) {
        lv_obj_set_width(s_pills[i], i == active ? 20 : 8);
        lv_obj_set_style_bg_color(s_pills[i], i == active ? COL_CPU : COL_PILL_OFF, 0);
    }
}

// `title` must be a string literal / static (compared by pointer).
void status_bar_set_title(const char *title) {
    if (title == s_title_text) return;
    s_title_text = title;
    lv_label_set_text(s_title, title);
}

void status_bar_set_dot(bool ok) {
    if ((int)ok == s_dot_ok) return;
    s_dot_ok = ok;
    lv_obj_set_style_bg_color(s_dot, ok ? COL_OK : COL_GRAY, 0);
}

void status_bar_set_wifi(bool ok) {
    if ((int)ok == s_wifi_ok) return;
    s_wifi_ok = ok;
    lv_obj_set_style_text_color(s_wifi, ok ? COL_OK : COL_GRAY, LV_PART_MAIN);
}
