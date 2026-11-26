#include "ui/ui_manager.h"

#include <Arduino.h>

#include "app_config.h"
#include "core/events.h"
#include "core/state.h"
#include "ui/components/status_bar.h"
#include "ui/screens/screen_home.h"
#include "ui/screens/screen_monitor.h"
#include "ui/screens/screen_weather.h"
#include "ui/screens/screen_wifi.h"
#include "ui/theme.h"

#define POLL_MS 250   // state-store poll; events make most updates immediate

static bool mqtt_dot() { return state_get_net().mqtt_connected; }

// The tiles, left to right. A new tab = one screen_* module + one row here.
struct Tile {
    const char *title;
    void (*create)(lv_obj_t *tile);
    void (*poll)();
    bool (*dot_ok)();   // status-bar dot for this tile
};

static const Tile kTiles[] = {
    {"Pi 5 MQTT", screen_monitor_create, screen_monitor_poll, mqtt_dot},
    {WEATHER_LOCATION, screen_weather_create, screen_weather_poll, screen_weather_ok},
    {"Home MQTT", screen_home_create, screen_home_poll, mqtt_dot},
};
static constexpr int TILE_COUNT = sizeof kTiles / sizeof kTiles[0];
enum { TILE_MONITOR = 0 };

static lv_obj_t *s_tv;
static lv_obj_t *s_tiles[TILE_COUNT];
static lv_timer_t *s_rotate_timer;
static lv_obj_t *s_main_scr, *s_wifi_scr;

static uint32_t s_net_ver = UINT32_MAX, s_setup_ver = UINT32_MAX;
static bool s_wifi_ok = false;
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
    status_bar_set_page(t);
    status_bar_set_title(kTiles[t].title);
    status_bar_set_dot(kTiles[t].dot_ok());
    status_bar_set_wifi(s_wifi_ok);
}

// ---------------- Wi-Fi setup screens ----------------

static void wifi_back() {
    s_wifi_dismissed = true;
    lv_tileview_set_tile_by_index(s_tv, TILE_MONITOR, 0, LV_ANIM_OFF);
    refresh_status();
    lv_screen_load(s_main_scr);
}

static void wifi_reopen() {
    s_wifi_dismissed = false;
    if (!s_wifi_setup_done) lv_screen_load(s_wifi_scr);
}

static void wifi_setup_apply(const WifiSetupInfo &info) {
    screen_wifi_apply(info);
    s_wifi_setup_done = info.state == WifiSetupState::Connected;
    if (s_wifi_setup_done) s_wifi_dismissed = false;
    lv_obj_t *want = (s_wifi_setup_done || s_wifi_dismissed) ? s_main_scr : s_wifi_scr;
    if (lv_screen_active() != want) lv_screen_load(want);
}

// ---------------- state polling ----------------

// Everything the UI shows comes from the state store. Each part redraws only
// when its group's version moved. Runs every POLL_MS and on every event.
static void poll_all() {
    if (state_version(StateGroup::WifiSetup) != s_setup_ver) {
        s_setup_ver = state_version(StateGroup::WifiSetup);
        wifi_setup_apply(state_get_wifi_setup());
    }
    if (state_version(StateGroup::Net) != s_net_ver) {
        s_net_ver = state_version(StateGroup::Net);
        const NetState ns = state_get_net();
        if (ns.wifi_connected != s_wifi_ok) {
            Serial.printf("[ui] status bar: wifi=%d (%s) mqtt=%d\n", ns.wifi_connected,
                          ns.wifi_connected ? "#3DDC84" : "#6B7788", ns.mqtt_connected);
        }
        s_wifi_ok = ns.wifi_connected;
    }
    for (const Tile &t : kTiles) t.poll();
    refresh_status();   // cheap: LVGL only redraws properties that changed
}

static void poll_timer_cb(lv_timer_t *) { poll_all(); }

// ---------------- navigation ----------------

static void next_tile() {
    int next = (active_tile() + 1) % TILE_COUNT;
    lv_tileview_set_tile_by_index(s_tv, next, 0, LV_ANIM_ON);
    refresh_status();   // set_tile updates the active tile immediately
    if (s_rotate_timer) lv_timer_reset(s_rotate_timer);
}

static void rotate_timer_cb(lv_timer_t *) { next_tile(); }

static void tile_changed_cb(lv_event_t *) {
    refresh_status();
    if (s_rotate_timer) lv_timer_reset(s_rotate_timer);
}

// ---------------- public ----------------

void ui_init() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, COL_TEXT, 0);
    lv_obj_set_style_text_font(scr, FONT_M, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_tv = lv_tileview_create(scr);
    lv_obj_set_size(s_tv, LV_PCT(100), lv_display_get_vertical_resolution(nullptr) - UI_STATUS_H);
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
        kTiles[i].create(s_tiles[i]);
    }

    status_bar_create(scr, TILE_COUNT, wifi_reopen);   // created last so it draws above the tileview
#if AUTO_ROTATE_MS > 0
    s_rotate_timer = lv_timer_create(rotate_timer_cb, AUTO_ROTATE_MS, nullptr);
#endif

    // Wi-Fi setup screens live on their own screen; the main UI stays hidden
    // until the setup flow reports Connected.
    s_main_scr = scr;
    s_wifi_scr = screen_wifi_create(wifi_back);
    lv_screen_load(s_wifi_scr);

    lv_timer_create(poll_timer_cb, POLL_MS, nullptr);
    poll_all();
}

void ui_task(void *) {
    for (;;) {
        AppEvent ev;
        bool dirty = false;
        while (events_receive(ev, 0)) {
            switch (ev.type) {
            case EventType::StateChanged: dirty = true; break;
            case EventType::NavNext:      next_tile(); break;
            case EventType::WifiReopen:   wifi_reopen(); break;
            }
        }
        if (dirty) poll_all();

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY || wait_ms > 20) wait_ms = 20;
        if (wait_ms < 2) wait_ms = 2;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}
