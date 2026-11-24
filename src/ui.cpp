#include "ui.h"

#include <Arduino.h>
#include <time.h>

#include "app_config.h"
#include "app_events.h"
#include "net_state.h"
#include "ui_home.h"
#include "ui_monitor.h"
#include "ui_theme.h"
#include "ui_weather.h"
#include "ui_wifi.h"

enum { TILE_MONITOR, TILE_WEATHER, TILE_HOME, TILE_COUNT };

static lv_obj_t *s_tv;
static lv_obj_t *s_tiles[TILE_COUNT];
static lv_obj_t *s_pills[TILE_COUNT];
static lv_obj_t *s_dot, *s_title, *s_wifi, *s_clock;
static lv_timer_t *s_rotate_timer;
static lv_obj_t *s_main_scr, *s_wifi_scr;

static bool s_wifi_ok = false;
static bool s_mqtt_ok = false;
static bool s_weather_ok = false;
// Set by the Back button on the Wi-Fi screens: stay on the main UI while the
// setup flow keeps running. Cleared on Connected, a BOOT long-press, or by
// tapping the Wi-Fi icon in the status bar.
static bool s_wifi_dismissed = false;
static bool s_wifi_setup_done = false;

static int active_tile() {
    lv_obj_t *t = lv_tileview_get_tile_active(s_tv);
    for (int i = 0; i < TILE_COUNT; i++)
        if (s_tiles[i] == t) return i;
    return 0;
}

static void refresh_status() {
    const int t = active_tile();
    for (int i = 0; i < TILE_COUNT; i++) {
        lv_obj_set_width(s_pills[i], i == t ? 20 : 8);
        lv_obj_set_style_bg_color(s_pills[i], i == t ? COL_CPU : COL_PILL_OFF, 0);
    }
    bool ok = s_mqtt_ok;
    switch (t) {
    case TILE_MONITOR: lv_label_set_text(s_title, "Pi 5 MQTT"); break;
    case TILE_WEATHER: lv_label_set_text(s_title, WEATHER_LOCATION); ok = s_weather_ok; break;
    case TILE_HOME:    lv_label_set_text(s_title, "Home MQTT"); break;
    }
    lv_obj_set_style_bg_color(s_dot, ok ? COL_OK : COL_GRAY, 0);
    lv_obj_set_style_text_color(s_wifi, s_wifi_ok ? COL_OK : COL_GRAY, 0);
}

// Pull the connection state (never pushed as events, so nothing can be lost).
// Runs every 500 ms, once at start-up and on Back, on the LVGL task.
static void net_poll() {
    static bool first = true;
    const NetState ns = net_state_get();
    const bool wifi_changed = first || ns.wifi_connected != s_wifi_ok;
    const bool mqtt_changed = first || ns.mqtt_connected != s_mqtt_ok;
    first = false;
    if (!wifi_changed && !mqtt_changed) return;

    s_wifi_ok = ns.wifi_connected;
    s_mqtt_ok = ns.mqtt_connected;
    if (mqtt_changed) ui_home_set_mqtt(s_mqtt_ok);
    refresh_status();
    Serial.printf("[ui] status bar: wifi=%d (%s) mqtt=%d\n", s_wifi_ok, s_wifi_ok ? "#3DDC84" : "#6B7788",
                  s_mqtt_ok);
}

static void net_timer_cb(lv_timer_t *) { net_poll(); }

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

// Wi-Fi icon tap: reopen the setup screens if they were dismissed with Back.
static void wifi_icon_cb(lv_event_t *) { ui_wifi_reopen(); }

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
    lv_obj_set_style_pad_column(left, 8, 0);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 0, 0);
    s_dot = ui_box(left);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_dot, COL_GRAY, 0);
    s_title = ui_label(left, FONT_M, COL_TEXT, "");

    // center: page pills
    lv_obj_t *pills = ui_box(bar);
    lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pills, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pills, 5, 0);
    lv_obj_align(pills, LV_ALIGN_CENTER, 0, 0);
    for (int i = 0; i < TILE_COUNT; i++) {
        s_pills[i] = ui_box(pills);
        lv_obj_set_size(s_pills[i], 8, 4);
        lv_obj_set_style_radius(s_pills[i], 2, 0);
        lv_obj_set_style_bg_opa(s_pills[i], LV_OPA_COVER, 0);
    }

    // right: wifi + clock
    lv_obj_t *right = ui_box(bar);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 8, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    s_wifi = ui_label(right, FONT_L, COL_GRAY, LV_SYMBOL_WIFI);   // ~20px glyph
    lv_obj_add_flag(s_wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_wifi, 6);
    lv_obj_add_event_cb(s_wifi, wifi_icon_cb, LV_EVENT_CLICKED, nullptr);
    s_clock = ui_label(right, FONT_M, COL_TEXT, "--:--");
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
        lv_dir_t dir = LV_DIR_HOR;
        if (i == 0) dir = LV_DIR_RIGHT;
        if (i == TILE_COUNT - 1) dir = LV_DIR_LEFT;
        s_tiles[i] = lv_tileview_add_tile(s_tv, i, 0, dir);
        lv_obj_set_style_pad_all(s_tiles[i], UI_PAD, 0);
        lv_obj_remove_flag(s_tiles[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    ui_monitor_create(s_tiles[TILE_MONITOR]);
    ui_weather_create(s_tiles[TILE_WEATHER]);
    ui_home_create(s_tiles[TILE_HOME]);

    status_bar_create(scr);   // created last so it draws above the tileview
    refresh_status();

    lv_timer_create(net_timer_cb, 500, nullptr);
    net_poll();
    lv_timer_create(clock_timer_cb, 1000, nullptr);
    clock_timer_cb(nullptr);
#if AUTO_ROTATE_MS > 0
    s_rotate_timer = lv_timer_create(rotate_timer_cb, AUTO_ROTATE_MS, nullptr);
#endif

    // Wi-Fi setup screens live on their own screen; the main UI stays hidden
    // until the setup flow reports Connected.
    s_main_scr = scr;
    s_wifi_scr = ui_wifi_create(ui_wifi_back);
    lv_screen_load(s_wifi_scr);
}

static void on_wifi_setup(const WifiSetupInfo &info) {
    ui_wifi_apply(info);
    if (info.state == WifiSetupState::Connected) {
        s_wifi_setup_done = true;
        s_wifi_dismissed = false;
    } else {
        s_wifi_setup_done = false;
    }
    lv_obj_t *want = (s_wifi_setup_done || s_wifi_dismissed) ? s_main_scr : s_wifi_scr;
    if (lv_screen_active() != want) lv_screen_load(want);
}

void ui_wifi_back() {
    s_wifi_dismissed = true;
    lv_tileview_set_tile_by_index(s_tv, TILE_MONITOR, 0, LV_ANIM_OFF);
    net_poll();
    refresh_status();
    lv_screen_load(s_main_scr);
}

void ui_wifi_reopen() {
    s_wifi_dismissed = false;
    if (!s_wifi_setup_done) lv_screen_load(s_wifi_scr);
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
        case AppEventType::WifiSetup:    on_wifi_setup(ev.wifi); net_poll(); break;
        case AppEventType::PiStats:      ui_monitor_update(ev.pi); break;
        case AppEventType::Services:
            ui_monitor_set_services(*ev.services);
            delete ev.services;
            break;
        case AppEventType::HomeConfig:   ui_home_set_config(ev.home); break;   // takes ownership
        case AppEventType::SwitchState:  ui_home_apply_state(ev.sw); break;
        case AppEventType::Weather:
            ui_weather_update(ev.weather);
            s_weather_ok = true;
            status_dirty = true;
            break;
        case AppEventType::WeatherError:
            ui_weather_set_error(ev.werr);
            s_weather_ok = false;
            status_dirty = true;
            break;
        }
    }
    if (status_dirty) refresh_status();
}
