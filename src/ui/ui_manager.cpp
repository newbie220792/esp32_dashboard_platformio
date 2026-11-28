#include "ui/ui_manager.h"

#include <Arduino.h>
#include <string.h>

#include "app_config.h"
#include "core/events.h"
#include "core/state.h"
#include "services/settings_service.h"
#include "ui/components/status_bar.h"
#include "ui/screens/screen_alerts.h"
#include "ui/screens/screen_home.h"
#include "ui/screens/screen_monitor.h"
#include "ui/screens/screen_settings.h"
#include "ui/screens/screen_sysinfo.h"
#include "ui/screens/screen_weather.h"
#include "ui/screens/screen_wifi.h"
#include "ui/theme.h"

#define POLL_MS         250   // state-store poll; events make most updates immediate
#define SCREEN_POWER_MS 500   // screen-off check

static bool mqtt_dot() { return state_get_net().mqtt_connected; }

// Weather tab title = the Pi 5's city (location_service), refreshed on poll.
static char s_city[CITY_LEN];
static const char *city_title() { return s_city; }
static const char *monitor_title() { return "Pi 5 MQTT"; }
static const char *home_title() { return "Home MQTT"; }

// The tiles, left to right. A new tab = one screen_* module + one row here.
struct Tile {
    const char *(*title)();
    void (*create)(lv_obj_t *tile);
    void (*poll)();
    bool (*dot_ok)();   // status-bar dot for this tile
};

static const Tile kTiles[] = {
    {monitor_title, screen_monitor_create, screen_monitor_poll, mqtt_dot},
    {city_title, screen_weather_create, screen_weather_poll, screen_weather_ok},
    {home_title, screen_home_create, screen_home_poll, mqtt_dot},
};
static constexpr int TILE_COUNT = sizeof kTiles / sizeof kTiles[0];
enum { TILE_MONITOR = 0 };

static lv_obj_t *s_tv;
static lv_obj_t *s_tiles[TILE_COUNT];
static lv_timer_t *s_rotate_timer;
static lv_obj_t *s_main_scr, *s_wifi_scr, *s_alerts_scr, *s_settings_scr;
static int s_return_tile = 0;   // tile to go back to from Alerts / Settings
static uint32_t s_loc_ver = UINT32_MAX, s_alerts_ver = UINT32_MAX;

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
    status_bar_set_title(kTiles[t].title());
    status_bar_set_dot(kTiles[t].dot_ok());
    status_bar_set_wifi(s_wifi_ok);
}

// ---------------- Alerts / Settings / System info overlays ----------------
// Own lv_screens, outside the swipe strip. Back returns to the tile left;
// System info (opened from Settings) goes back to Settings.

static bool overlay_active() {
    lv_obj_t *a = lv_screen_active();
    return a == s_alerts_scr || a == s_settings_scr || (a && a == screen_sysinfo_get());
}

// Every screen change except Settings <-> System info: System info exists only
// while shown, so it is deleted once another screen is active.
static void show_screen(lv_obj_t *scr) {
    lv_screen_load(scr);
    screen_sysinfo_drop();
}

static void leave_overlay() {
    if (lv_screen_active() == s_alerts_scr) screen_alerts_close();
}

static void overlay_open(lv_obj_t *scr) {
    if (lv_screen_active() == scr) return;
    if (!overlay_active()) s_return_tile = active_tile();
    leave_overlay();
    if (scr == s_alerts_scr) screen_alerts_open();
    else screen_settings_poll();
    show_screen(scr);
}

static void overlay_back() {
    leave_overlay();
    lv_tileview_set_tile_by_index(s_tv, s_return_tile, 0, LV_ANIM_OFF);
    refresh_status();
    show_screen(s_main_scr);
}

static void open_alerts() { overlay_open(s_alerts_scr); }
static void open_settings() { overlay_open(s_settings_scr); }

// System info: built on open, slides in from the right; Back slides it out
// and LVGL deletes it at the end of the animation (auto_del).
static void sysinfo_back() {
    screen_settings_poll();
    lv_screen_load_anim(s_settings_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, SCREEN_ANIM_MS, 0, true);
}

static void open_sysinfo() {
    if (screen_sysinfo_get()) return;   // already open, or still sliding out
    lv_screen_load_anim(screen_sysinfo_create(sysinfo_back), LV_SCR_LOAD_ANIM_MOVE_LEFT, SCREEN_ANIM_MS, 0, false);
}

// ---------------- Wi-Fi setup screens ----------------

static void wifi_back() {
    s_wifi_dismissed = true;
    lv_tileview_set_tile_by_index(s_tv, TILE_MONITOR, 0, LV_ANIM_OFF);
    refresh_status();
    show_screen(s_main_scr);
}

static void wifi_reopen() {
    s_wifi_dismissed = false;
    if (s_wifi_setup_done) return;
    leave_overlay();
    show_screen(s_wifi_scr);
}

static void wifi_setup_apply(const WifiSetupInfo &info) {
    screen_wifi_apply(info);
    s_wifi_setup_done = info.state == WifiSetupState::Connected;
    if (s_wifi_setup_done) s_wifi_dismissed = false;
    const bool want_main = s_wifi_setup_done || s_wifi_dismissed;
    if (want_main && overlay_active()) return;   // Alerts / Settings / System info count as the main UI
    lv_obj_t *want = want_main ? s_main_scr : s_wifi_scr;
    if (lv_screen_active() != want) {
        if (!want_main) leave_overlay();
        show_screen(want);
    }
}

// ---------------- screen off (Settings) ----------------
// LVGL's inactivity time drives the backlight: dim SCREEN_DIM_LEAD_MS before
// the delay, then off. While dimmed/off a transparent shield on the top layer
// eats the first touch, so waking never presses the button under the finger.

static lv_obj_t *s_shield;
static ScreenPower s_power = ScreenPower::On;

static void shield_cb(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_delete_async(s_shield);   // the press that woke us is over
        s_shield = nullptr;
    }
}

static void shield_up() {
    if (s_shield) return;
    s_shield = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_shield);
    lv_obj_set_size(s_shield, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(s_shield, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_shield, shield_cb, LV_EVENT_ALL, nullptr);
}

static void power_set(ScreenPower p) {
    if (p == s_power) return;
    s_power = p;
    settings_service_set_power(p);
    if (p != ScreenPower::On) shield_up();
}

static void power_timer_cb(lv_timer_t *) {
    const uint32_t sleep_ms = state_get_settings().sleep_s * 1000UL;
    const uint32_t idle = lv_display_get_inactive_time(nullptr);
    ScreenPower p = ScreenPower::On;
    if (sleep_ms && idle >= sleep_ms) p = ScreenPower::Off;
    else if (sleep_ms && idle + SCREEN_DIM_LEAD_MS >= sleep_ms) p = ScreenPower::Dim;
    power_set(p);   // a touch resets `idle`, so the light comes back on press
}

// BOOT short press: wake only (no tile change) when the screen is dim/off.
static bool wake_by_button() {
    if (s_power == ScreenPower::On) return false;
    lv_display_trigger_activity(nullptr);
    power_set(ScreenPower::On);
    if (s_shield) {   // no finger on the glass: drop the shield now
        lv_obj_delete(s_shield);
        s_shield = nullptr;
    }
    return true;
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
    if (state_version(StateGroup::Location) != s_loc_ver) {
        s_loc_ver = state_version(StateGroup::Location);
        const LocationState ls = state_get_location();
        memcpy(s_city, ls.loc.city, sizeof s_city);
        s_city[sizeof s_city - 1] = '\0';
    }
    if (state_version(StateGroup::Alerts) != s_alerts_ver) {
        s_alerts_ver = state_version(StateGroup::Alerts);
        static AlertsState st;   // LVGL task only (~1.4 KB, off the stack)
        state_get_alerts(st);
        status_bar_set_alerts(st.have_summary && st.summary.unacked > 0);   // offline: last value stays
    }
    for (const Tile &t : kTiles) t.poll();
    screen_alerts_poll();
    screen_settings_poll();
    screen_sysinfo_poll();
    refresh_status();   // cheap: LVGL only redraws properties that changed
}

static void poll_timer_cb(lv_timer_t *) { poll_all(); }

// ---------------- navigation ----------------

static void next_tile() {
    if (overlay_active()) return;   // overlays sit outside the swipe strip
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

    // created last so it draws above the tileview
    status_bar_create(scr, TILE_COUNT, StatusBarActions{wifi_reopen, open_alerts, open_settings});
#if AUTO_ROTATE_MS > 0
    s_rotate_timer = lv_timer_create(rotate_timer_cb, AUTO_ROTATE_MS, nullptr);
#endif

    // Wi-Fi setup screens live on their own screen; the main UI stays hidden
    // until the setup flow reports Connected.
    s_main_scr = scr;
    s_wifi_scr = screen_wifi_create(wifi_back);
    s_alerts_scr = screen_alerts_create(overlay_back);
    s_settings_scr = screen_settings_create(overlay_back, open_sysinfo);
    lv_screen_load(s_wifi_scr);

    lv_timer_create(poll_timer_cb, POLL_MS, nullptr);
    lv_timer_create(power_timer_cb, SCREEN_POWER_MS, nullptr);
    poll_all();
}

void ui_task(void *) {
    for (;;) {
        AppEvent ev;
        bool dirty = false;
        while (events_receive(ev, 0)) {
            switch (ev.type) {
            case EventType::StateChanged: dirty = true; break;
            case EventType::NavNext:
                if (!wake_by_button()) next_tile();
                break;
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
