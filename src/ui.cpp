#include "ui.h"

#include <Arduino.h>
#include <time.h>

#include "app_config.h"
#include "app_events.h"
#include "ui_monitor.h"
#include "ui_theme.h"
#include "ui_weather.h"

#define TILE_COUNT 2

static lv_obj_t *s_tv;
static lv_obj_t *s_tiles[TILE_COUNT];
static lv_obj_t *s_pills[TILE_COUNT];
static lv_obj_t *s_dot, *s_title, *s_wifi, *s_clock;
static lv_timer_t *s_rotate_timer;

static bool s_wifi_ok = false;
static bool s_mqtt_ok = false;
static bool s_weather_ok = false;

static int active_tile() {
    lv_obj_t *t = lv_tileview_get_tile_active(s_tv);
    for (int i = 0; i < TILE_COUNT; i++)
        if (s_tiles[i] == t) return i;
    return 0;
}

static void refresh_status() {
    const int t = active_tile();
    for (int i = 0; i < TILE_COUNT; i++) {
        lv_obj_set_width(s_pills[i], i == t ? 16 : 6);
        lv_obj_set_style_bg_color(s_pills[i], i == t ? COL_CPU : COL_PILL_OFF, 0);
    }
    if (t == 0) {
        lv_label_set_text(s_title, "Pi 5 MQTT");
        lv_obj_set_style_bg_color(s_dot, s_mqtt_ok ? COL_OK : COL_GRAY, 0);
    } else {
        lv_label_set_text(s_title, WEATHER_LOCATION);
        lv_obj_set_style_bg_color(s_dot, s_weather_ok ? COL_OK : COL_GRAY, 0);
    }
    lv_obj_set_style_text_color(s_wifi, s_wifi_ok ? COL_OK : COL_GRAY, 0);
}

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

static void rotate_timer_cb(lv_timer_t *) { ui_next_tile(); }

static void tile_changed_cb(lv_event_t *) {
    refresh_status();
    if (s_rotate_timer) lv_timer_reset(s_rotate_timer);
}

static void status_bar_create(lv_obj_t *scr) {
    lv_obj_t *bar = ui_box(scr);
    lv_obj_set_size(bar, SCREEN_W, UI_STATUS_H);
    lv_obj_set_style_pad_hor(bar, UI_PAD, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    // left: dot + label
    lv_obj_t *left = ui_box(bar);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 6, 0);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 0, 0);
    s_dot = ui_box(left);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dot, COL_GRAY, 0);
    s_title = ui_label(left, FONT_S, COL_TEXT, "");

    // center: page pills
    lv_obj_t *pills = ui_box(bar);
    lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pills, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pills, 4, 0);
    lv_obj_align(pills, LV_ALIGN_CENTER, 0, 0);
    for (int i = 0; i < TILE_COUNT; i++) {
        s_pills[i] = ui_box(pills);
        lv_obj_set_size(s_pills[i], 6, 4);
        lv_obj_set_style_radius(s_pills[i], 2, 0);
        lv_obj_set_style_bg_opa(s_pills[i], LV_OPA_COVER, 0);
    }

    // right: wifi + clock
    lv_obj_t *right = ui_box(bar);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 6, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    s_wifi = ui_label(right, FONT_S, COL_GRAY, LV_SYMBOL_WIFI);
    s_clock = ui_label(right, FONT_S, COL_TEXT, "--:--");
}

void ui_init() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, COL_TEXT, 0);
    lv_obj_set_style_text_font(scr, FONT_M, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_tv = lv_tileview_create(scr);
    lv_obj_set_size(s_tv, SCREEN_W, SCREEN_H - UI_STATUS_H);
    lv_obj_align(s_tv, LV_ALIGN_TOP_LEFT, 0, UI_STATUS_H);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tv, tile_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    for (int i = 0; i < TILE_COUNT; i++) {
        lv_dir_t dir = (i == 0) ? LV_DIR_RIGHT : LV_DIR_LEFT;
        s_tiles[i] = lv_tileview_add_tile(s_tv, i, 0, dir);
        lv_obj_set_style_pad_all(s_tiles[i], UI_PAD, 0);
        lv_obj_remove_flag(s_tiles[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    ui_monitor_create(s_tiles[0]);
    ui_weather_create(s_tiles[1]);

    status_bar_create(scr);   // created last so it draws above the tileview
    refresh_status();

    lv_timer_create(clock_timer_cb, 1000, nullptr);
    clock_timer_cb(nullptr);
#if AUTO_ROTATE_MS > 0
    s_rotate_timer = lv_timer_create(rotate_timer_cb, AUTO_ROTATE_MS, nullptr);
#endif
}

void ui_next_tile() {
    int next = (active_tile() + 1) % TILE_COUNT;
    lv_tileview_set_tile_by_index(s_tv, next, 0, LV_ANIM_ON);
    refresh_status();   // set_tile updates the active tile immediately
    if (s_rotate_timer) lv_timer_reset(s_rotate_timer);
}

void ui_process_events() {
    AppEvent ev;
    bool status_dirty = false;
    while (app_events_receive(ev, 0)) {
        switch (ev.type) {
        case AppEventType::WifiState:    s_wifi_ok = ev.flag; status_dirty = true; break;
        case AppEventType::MqttState:    s_mqtt_ok = ev.flag; status_dirty = true; break;
        case AppEventType::PiStats:      ui_monitor_update(ev.pi); break;
        case AppEventType::Weather:
            ui_weather_update(ev.weather);
            s_weather_ok = true;
            status_dirty = true;
            break;
        case AppEventType::WeatherError: s_weather_ok = false; status_dirty = true; break;
        }
    }
    if (status_dirty) refresh_status();
}
