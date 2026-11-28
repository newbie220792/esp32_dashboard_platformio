#include "ui/components/status_bar.h"

#include <string.h>
#include <time.h>

#include "ui/theme.h"

#define MAX_PAGES  8
#define ICON_BOX   28   // bell / gear frame
#define ICON_HIT   6    // extra touch margin each side: 28 + 2*6 = 40px target
#define BADGE      10

static lv_obj_t *s_pills[MAX_PAGES];
static int s_pages;
static lv_obj_t *s_dot, *s_title, *s_wifi, *s_clock, *s_badge;
static StatusBarActions s_actions;
// Last values set: the setters are called on every UI poll, so skip no-ops
// (each LVGL set would invalidate and redraw the bar).
static int s_page = -1, s_dot_ok = -1, s_wifi_ok = -1, s_badge_on = -1;
static char s_title_text[40];

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

static void tap_cb(lv_event_t *e) {
    auto fn = reinterpret_cast<void (*)()>(lv_event_get_user_data(e));
    if (fn) fn();
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

// 28px tappable frame with a 20px symbol, pressed = card colour.
static lv_obj_t *icon_button(lv_obj_t *parent, const char *symbol, void (*fn)()) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, ICON_BOX, ICON_BOX);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, COL_CARD, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, ICON_HIT);
    lv_obj_add_event_cb(b, tap_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(fn));
    lv_obj_t *l = ui_label(b, FONT_L, COL_TEXT, symbol);
    lv_obj_center(l);
    return b;
}

void status_bar_create(lv_obj_t *scr, int page_count, const StatusBarActions &actions) {
    s_pages = page_count < MAX_PAGES ? page_count : MAX_PAGES;
    s_actions = actions;

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
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s_title, 140, 0);   // stays clear of the centred pills

    // center: page pills
    lv_obj_t *pills = row(bar, LV_FLEX_ALIGN_CENTER, 5, LV_ALIGN_CENTER);
    for (int i = 0; i < s_pages; i++) {
        s_pills[i] = ui_box(pills);
        lv_obj_set_size(s_pills[i], 8, 4);
        lv_obj_set_style_radius(s_pills[i], 2, 0);
        lv_obj_set_style_bg_opa(s_pills[i], LV_OPA_COVER, 0);
    }

    // right: bell + gear + wifi + clock
    // START, not END: in a content-sized row END places the children at negative
    // x, LVGL then sizes the row to the clock only and clips bell/gear/Wi-Fi.
    // LV_ALIGN_RIGHT_MID already puts the whole row on the right edge.
    lv_obj_t *right = row(bar, LV_FLEX_ALIGN_START, 8, LV_ALIGN_RIGHT_MID);
    lv_obj_t *bell = icon_button(right, LV_SYMBOL_BELL, s_actions.on_alerts_tap);
    s_badge = ui_box(bell);   // red dot, #0B0F14 ring so it reads over the glyph
    lv_obj_set_size(s_badge, BADGE, BADGE);
    lv_obj_set_style_radius(s_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_badge, COL_STOPPED, 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_badge, COL_BG, 0);
    lv_obj_set_style_border_width(s_badge, 2, 0);
    lv_obj_set_style_border_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_align(s_badge, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
    icon_button(right, LV_SYMBOL_SETTINGS, s_actions.on_settings_tap);

    s_wifi = ui_label(right, FONT_L, COL_GRAY, LV_SYMBOL_WIFI);   // ~20px glyph
    lv_obj_add_flag(s_wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_wifi, 6);
    lv_obj_add_event_cb(s_wifi, tap_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(s_actions.on_wifi_tap));
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

void status_bar_set_title(const char *title) {
    if (!strcmp(title, s_title_text)) return;
    strncpy(s_title_text, title, sizeof s_title_text - 1);
    lv_label_set_text(s_title, s_title_text);
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

void status_bar_set_alerts(bool show_badge) {
    if ((int)show_badge == s_badge_on) return;
    s_badge_on = show_badge;
    if (show_badge) lv_obj_remove_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
}
