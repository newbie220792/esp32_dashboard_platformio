#include "ui_wifi.h"

#include <initializer_list>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "icon_draw.h"
#include "parsers.h"
#include "secrets.h"
#include "ui_theme.h"
#include "wifi_setup.h"

#ifndef SETUP_AP_PASSWORD
#define SETUP_AP_PASSWORD ""
#endif

#define BAR_H     36   // top bar with "Back"
#define BACK_H    32
#define BTN_H     44
#define STATUS_H  36   // "Waiting for setup..." card
#define QR_FRAME  136
#define BULLET "\xE2\x80\xA2"

static lv_obj_t *s_scr, *s_bar_title;
static void (*s_on_back)();
static lv_obj_t *v_connect, *v_fail, *v_portal, *v_retry;

// connecting
static lv_obj_t *c_title, *c_net;
// screen 4
static lv_obj_t *f_net, *f_reason, *f_countdown, *f_progress, *f_btn_retry, *f_btn_setup;
static lv_timer_t *s_countdown_timer;
static uint32_t s_countdown_start;
// screen 6
static lv_obj_t *r_net, *r_reason, *r_attempt;

// ---------------- pieces ----------------

static void wifi_off_draw_cb(lv_event_t *e) {
    IconCtx c = icon_ctx(e);
    const lv_color_t red = COL_STOPPED;
    icon_arc(c, 50, 82, 64, 10, 225, 315, red);
    icon_arc(c, 50, 82, 42, 10, 225, 315, red);
    icon_arc(c, 50, 82, 20, 10, 225, 315, red);
    icon_circle(c, 50, 82, 7, red);
    icon_line(c, 14, 10, 86, 90, 20, c.bg);   // gap around the slash
    icon_line(c, 14, 10, 86, 90, 9, red);
}

static lv_obj_t *wifi_off_icon(lv_obj_t *parent, int32_t size) {
    lv_obj_t *o = ui_box(parent);
    lv_obj_set_size(o, size, size);
    lv_obj_add_event_cb(o, wifi_off_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    return o;
}

static lv_obj_t *view_create() {
    lv_obj_t *v = ui_box(s_scr);
    lv_obj_set_size(v, SCREEN_W, SCREEN_H - BAR_H);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, 0, BAR_H);
    lv_obj_set_style_pad_all(v, UI_PAD, 0);
    lv_obj_set_flex_flow(v, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(v, UI_GAP, 0);
    lv_obj_add_flag(v, LV_OBJ_FLAG_HIDDEN);
    return v;
}

// Vertically centred column that takes the free space of a view.
static lv_obj_t *center_group(lv_obj_t *view) {
    lv_obj_t *g = ui_box(view);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_flex_grow(g, 1);
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(g, 4, 0);
    return g;
}

static lv_obj_t *waiting_card(lv_obj_t *view) {
    lv_obj_t *card = ui_card(view, 0);
    lv_obj_set_size(card, LV_PCT(100), STATUS_H);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 8, 0);
    lv_obj_t *dot = ui_box(card);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_TEMP, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    ui_label(card, FONT_M, COL_TEXT, "Waiting for setup...");
    return card;
}

static void set_net(lv_obj_t *label, const char *ssid) {
    lv_label_set_text_fmt(label, "Network: %s", ssid);
}

// ---------------- connecting ----------------

static void connect_view_create() {
    v_connect = view_create();
    lv_obj_t *g = center_group(v_connect);
    lv_obj_set_style_pad_row(g, 8, 0);
    lv_obj_t *sp = lv_spinner_create(g);
    lv_obj_set_size(sp, 44, 44);
    lv_spinner_set_anim_params(sp, 1000, 270);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sp, COL_CPU, LV_PART_INDICATOR);
    c_title = ui_label(g, FONT_L, COL_TEXT, "Connecting...");
    c_net = ui_label(g, FONT_M, COL_MUTED, "");
    lv_label_set_long_mode(c_net, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(c_net, 420, 0);
}

// ---------------- screen 4 ----------------

static void buttons_enable(bool en) {
    for (lv_obj_t *b : {f_btn_retry, f_btn_setup}) {
        if (en) lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        else lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(b, en ? LV_OPA_COVER : LV_OPA_50, 0);
    }
}

static void fail_btn_cb(lv_event_t *e) {
    WifiCmd cmd = (WifiCmd)(uintptr_t)lv_event_get_user_data(e);
    if (wifi_setup_command(cmd)) buttons_enable(false);   // until the next state arrives
}

static lv_obj_t *button(lv_obj_t *row, const char *text, lv_color_t bg, lv_color_t fg, WifiCmd cmd) {
    lv_obj_t *b = ui_box(row);
    lv_obj_set_height(b, LV_PCT(100));
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, UI_RADIUS, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = ui_label(b, FONT_MD, fg, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, fail_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)cmd);
    return b;
}

static void countdown_cb(lv_timer_t *) {
    uint32_t el = lv_tick_elaps(s_countdown_start);
    if (el >= WIFI_FAIL_COUNTDOWN_MS) {
        lv_label_set_text(f_countdown, "Opening setup...");
        lv_bar_set_value(f_progress, 1000, LV_ANIM_OFF);
        lv_timer_pause(s_countdown_timer);
        return;
    }
    uint32_t left_s = (WIFI_FAIL_COUNTDOWN_MS - el + 999) / 1000;
    lv_label_set_text_fmt(f_countdown, "Setup starts in %lus", (unsigned long)left_s);
    lv_bar_set_value(f_progress, (int32_t)(el * 1000 / WIFI_FAIL_COUNTDOWN_MS), LV_ANIM_OFF);
}

static void fail_view_create() {
    v_fail = view_create();
    lv_obj_t *g = center_group(v_fail);
    wifi_off_icon(g, 44);
    ui_label(g, FONT_L_BOLD, COL_TEXT, "Can't connect");
    f_net = ui_label(g, FONT_M, COL_MUTED, "");
    f_reason = ui_label(g, FONT_M_BOLD, COL_STOPPED, "");

    f_countdown = ui_label(v_fail, FONT_S, COL_MUTED, "");
    f_progress = lv_bar_create(v_fail);
    lv_obj_set_size(f_progress, LV_PCT(100), 4);
    lv_bar_set_range(f_progress, 0, 1000);
    lv_obj_set_style_radius(f_progress, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(f_progress, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(f_progress, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(f_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(f_progress, COL_CPU, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(f_progress, 0, LV_PART_MAIN);

    lv_obj_t *row = ui_box(v_fail);
    lv_obj_set_size(row, LV_PCT(100), BTN_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    f_btn_retry = button(row, "Retry", COL_SW_OFF, COL_TEXT, WifiCmd::Retry);
    f_btn_setup = button(row, "Setup now", COL_CPU, COL_ON_ACCENT, WifiCmd::SetupNow);

    s_countdown_timer = lv_timer_create(countdown_cb, 100, nullptr);
    lv_timer_pause(s_countdown_timer);
}

// ---------------- screen 5 ----------------

static void portal_view_create() {
    v_portal = view_create();
    lv_obj_t *row = ui_box(v_portal);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 20, 0);

    // QR in a white frame (quiet zone for scanners)
    lv_obj_t *frame = ui_box(row);
    lv_obj_set_size(frame, QR_FRAME, QR_FRAME);
    lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(frame, 8, 0);
    lv_obj_t *qr = lv_qrcode_create(frame);
    lv_qrcode_set_size(qr, QR_FRAME - 14);
    lv_qrcode_set_dark_color(qr, COL_BG);
    lv_qrcode_set_light_color(qr, lv_color_white());
    char payload[128];
    if (build_wifi_qr(payload, sizeof payload, SETUP_AP_SSID, SETUP_AP_PASSWORD))
        lv_qrcode_update(qr, payload, strlen(payload));
    lv_obj_center(qr);

    lv_obj_t *col = ui_box(row);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    ui_label(col, FONT_M, COL_MUTED, "Join this network");
    ui_label(col, FONT_L_BOLD, COL_CPU, SETUP_AP_SSID);
    if (strlen(SETUP_AP_PASSWORD)) {
        ui_label(col, FONT_S, COL_MUTED, "Password");
        ui_label(col, FONT_MD_BOLD, COL_CPU, SETUP_AP_PASSWORD);
    }
    lv_obj_t *then = ui_label(col, FONT_M, COL_MUTED, "Then open");
    lv_obj_set_style_pad_top(then, 6, 0);
    ui_label(col, FONT_L_BOLD, COL_TEXT, SETUP_AP_IP);
    lv_obj_t *hint = ui_label(col, FONT_S, COL_MUTED, "The page may open automatically.");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_pad_top(hint, 6, 0);

    waiting_card(v_portal);
}

// ---------------- screen 6 ----------------

static void retry_view_create() {
    v_retry = view_create();
    lv_obj_t *g = center_group(v_retry);
    wifi_off_icon(g, 32);
    ui_label(g, FONT_L_BOLD, COL_TEXT, "Couldn't connect");
    r_net = ui_label(g, FONT_M, COL_MUTED, "");
    r_reason = ui_label(g, FONT_M_BOLD, COL_STOPPED, "");

    lv_obj_t *info = ui_card(v_retry, 0);
    lv_obj_set_size(info, LV_PCT(100), 52);
    lv_obj_set_style_pad_hor(info, 12, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(info, 4, 0);
    lv_obj_t *line1 = ui_box(info);
    lv_obj_set_size(line1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_t *open = ui_label(line1, FONT_M, COL_TEXT, "Setup is open again");
    lv_obj_align(open, LV_ALIGN_LEFT_MID, 0, 0);
    r_attempt = ui_label(line1, FONT_M, COL_MUTED, "");
    lv_obj_align(r_attempt, LV_ALIGN_RIGHT_MID, 0, 0);
    ui_label(info, FONT_S_BOLD, COL_CPU, SETUP_AP_SSID " " BULLET " " SETUP_AP_IP);

    waiting_card(v_retry);
}

// ---------------- public ----------------

static void back_cb(lv_event_t *) {
    if (s_on_back) s_on_back();
}

lv_obj_t *ui_wifi_create(void (*on_back)()) {
    s_on_back = on_back;
    s_scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_scr, COL_TEXT, 0);
    lv_obj_set_style_text_font(s_scr, FONT_M, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Top bar: "‹ Back" (to tile 1), screen title, gray Wi-Fi icon right
    lv_obj_t *bar = ui_box(s_scr);
    lv_obj_set_size(bar, SCREEN_W, BAR_H);
    lv_obj_set_style_pad_hor(bar, UI_PAD, 0);
    lv_obj_set_style_pad_top(bar, 4, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 10, 0);

    lv_obj_t *back = ui_box(bar);
    lv_obj_set_size(back, LV_SIZE_CONTENT, BACK_H);
    lv_obj_set_style_pad_hor(back, 12, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_set_style_bg_color(back, COL_CARD, 0);
    lv_obj_set_style_bg_color(back, COL_SW_OFF, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *back_l = ui_label(back, FONT_M_BOLD, COL_TEXT, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_l);

    s_bar_title = ui_label(bar, FONT_M, COL_TEXT, "Wi-Fi");
    lv_obj_set_flex_grow(s_bar_title, 1);
    ui_label(bar, FONT_L, COL_GRAY, LV_SYMBOL_WIFI);

    connect_view_create();
    fail_view_create();
    portal_view_create();
    retry_view_create();

    WifiSetupInfo init{};
    init.state = WifiSetupState::Connecting;
    ui_wifi_apply(init);
    return s_scr;
}

void ui_wifi_apply(const WifiSetupInfo &info) {
    lv_obj_t *show = v_connect;
    const char *bar = "Wi-Fi setup";
    lv_timer_pause(s_countdown_timer);

    switch (info.state) {
    case WifiSetupState::Connecting:
    case WifiSetupState::ConnectingNew:
    case WifiSetupState::Connected:
        if (info.state != WifiSetupState::ConnectingNew) bar = "Wi-Fi";
        lv_label_set_text(c_title, info.state == WifiSetupState::Connected ? "Connected" : "Connecting...");
        if (info.ssid[0]) set_net(c_net, info.ssid);
        else lv_label_set_text(c_net, "");
        break;
    case WifiSetupState::FailedSaved:
        bar = "Wi-Fi";
        show = v_fail;
        set_net(f_net, info.ssid);
        lv_label_set_text(f_reason, wifi_fail_reason_text(info.reason));
        buttons_enable(true);
        s_countdown_start = lv_tick_get();
        countdown_cb(nullptr);
        lv_timer_resume(s_countdown_timer);
        break;
    case WifiSetupState::Portal:
        show = v_portal;
        break;
    case WifiSetupState::FailedNew:
        show = v_retry;
        set_net(r_net, info.ssid);
        lv_label_set_text(r_reason, wifi_fail_reason_text(info.reason));
        lv_label_set_text_fmt(r_attempt, "Attempt %u", (unsigned)info.attempt);
        break;
    }

    lv_label_set_text(s_bar_title, bar);
    for (lv_obj_t *v : {v_connect, v_fail, v_portal, v_retry}) {
        if (v == show) lv_obj_remove_flag(v, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(v, LV_OBJ_FLAG_HIDDEN);
    }
}
