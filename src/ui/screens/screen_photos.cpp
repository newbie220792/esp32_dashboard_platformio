#include "ui/screens/screen_photos.h"

#include <stdio.h>
#include <time.h>

#include "core/state.h"
#include "services/photo_service.h"
#include "ui/theme.h"

#define FADE_MS       300
#define CONTROLS_MS   3000   // controls hide after this long without a tap
#define TOP_H         56
#define BOTTOM_H      84
#define CHIP_H        22
#define MAX_PAGES     8
#define SHADE_OPA     204    // rgba(11,15,20,.8)
#define CHIP_OPA      140    // rgba(11,15,20,.55)

static lv_obj_t *s_tile;
static lv_obj_t *s_img[2];            // s_img[k] always shows frames[k]
static lv_image_dsc_t s_dsc[2];
static lv_obj_t *s_counter_chip, *s_counter;   // bottom-right "3 / 24" (controls hidden)
static lv_obj_t *s_offline;           // top-right chip
static lv_obj_t *s_empty, *s_empty_text, *s_spinner;
static lv_obj_t *s_top, *s_bottom, *s_pills[MAX_PAGES], *s_wifi, *s_clock;
static lv_obj_t *s_dwell_label, *s_play_label, *s_ctrl_counter;
static lv_timer_t *s_hide_timer, *s_clock_timer;
static int s_pages;

static PhotoState s_ps;               // last state drawn
static uint32_t s_ver = UINT32_MAX, s_net_ver = UINT32_MAX;
static uint32_t s_seq_shown = 0;
static bool s_visible, s_controls;

// ---------------- helpers ----------------

static void set_hidden(lv_obj_t *o, bool hidden) {
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *chip(lv_obj_t *parent) {
    lv_obj_t *c = ui_box(parent);
    lv_obj_set_size(c, LV_SIZE_CONTENT, CHIP_H);
    lv_obj_set_style_radius(c, CHIP_H / 2, 0);
    lv_obj_set_style_bg_color(c, COL_BG, 0);
    lv_obj_set_style_bg_opa(c, CHIP_OPA, 0);
    lv_obj_set_style_pad_hor(c, 9, 0);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c, 6, 0);
    return c;
}

// Vertical shade: `top_dark` = dark at the top fading to clear (top bar), else the reverse.
static lv_obj_t *shade(lv_obj_t *parent, int32_t h, bool top_dark) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, LV_PCT(100), h);
    lv_obj_set_style_bg_color(b, COL_BG, 0);
    lv_obj_set_style_bg_grad_color(b, COL_BG, 0);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_opa(b, top_dark ? SHADE_OPA : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_grad_opa(b, top_dark ? LV_OPA_TRANSP : SHADE_OPA, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_align(b, top_dark ? LV_ALIGN_TOP_MID : LV_ALIGN_BOTTOM_MID, 0, 0);
    return b;
}

static lv_obj_t *round_button(lv_obj_t *parent, int32_t size, const char *symbol, lv_color_t bg, lv_opa_t bg_opa,
                              lv_color_t fg, PhotoCmd cmd, lv_obj_t **label_out = nullptr);

// ---------------- controls ----------------

static void controls_show(bool show) {
    if (show == s_controls) return;
    s_controls = show;
    set_hidden(s_top, !show);
    set_hidden(s_bottom, !show);
    set_hidden(s_counter_chip, show || !s_ps.count || !s_ps.have_frame);
    lv_obj_align(s_offline, LV_ALIGN_TOP_RIGHT, -10, show ? TOP_H + 4 : 10);
    if (show) {
        lv_timer_reset(s_hide_timer);
        lv_timer_resume(s_hide_timer);
    } else {
        lv_timer_pause(s_hide_timer);
    }
    photo_service_command(show ? PhotoCmd::Hold : PhotoCmd::Release);   // no slide change while the user is at the buttons
}

static void hide_timer_cb(lv_timer_t *) { controls_show(false); }

// Tap on the photo (not on a button): toggle the controls.
static void tile_clicked_cb(lv_event_t *) { controls_show(!s_controls); }

static void button_cb(lv_event_t *e) {
    photo_service_command((PhotoCmd)(uintptr_t)lv_event_get_user_data(e));
    lv_timer_reset(s_hide_timer);   // every button tap restarts the 3 s
}

static lv_obj_t *round_button(lv_obj_t *parent, int32_t size, const char *symbol, lv_color_t bg, lv_opa_t bg_opa,
                              lv_color_t fg, PhotoCmd cmd, lv_obj_t **label_out) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, size, size);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, bg_opa, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(b, size < 44 ? (44 - size) / 2 : 0);   // 44 px touch target
    lv_obj_t *l = ui_label(b, FONT_L, fg, symbol);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, button_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)cmd);
    if (label_out) *label_out = l;
    return b;
}

static void clock_cb(lv_timer_t *) {
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    if (tmv.tm_year + 1900 < 2024) lv_label_set_text(s_clock, "--:--");
    else lv_label_set_text_fmt(s_clock, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
}

static void top_bar_create() {
    s_top = shade(s_tile, TOP_H, true);
    lv_obj_set_style_pad_hor(s_top, UI_PAD, 0);
    lv_obj_set_style_pad_bottom(s_top, TOP_H - UI_STATUS_H, 0);   // content on the first 28 px, like the status bar

    lv_obj_t *title = ui_label(s_top, FONT_M, COL_TEXT, "Photos");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *pills = ui_box(s_top);
    lv_obj_set_size(pills, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(pills, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pills, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pills, 6, 0);
    lv_obj_align(pills, LV_ALIGN_CENTER, 0, 0);
    for (int i = 0; i < MAX_PAGES; i++) {
        s_pills[i] = ui_box(pills);
        lv_obj_set_size(s_pills[i], 8, 4);
        lv_obj_set_style_radius(s_pills[i], 2, 0);
        lv_obj_set_style_bg_opa(s_pills[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(s_pills[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *right = ui_box(s_top);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 8, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    s_wifi = ui_label(right, FONT_L, COL_GRAY, LV_SYMBOL_WIFI);
    s_clock = ui_label(right, FONT_M, COL_TEXT, "--:--");
    s_clock_timer = lv_timer_create(clock_cb, 1000, nullptr);
    clock_cb(nullptr);
}

static void bottom_bar_create() {
    s_bottom = shade(s_tile, BOTTOM_H, false);
    lv_obj_set_style_pad_hor(s_bottom, 12, 0);
    lv_obj_set_style_pad_top(s_bottom, BOTTOM_H - 64, 0);   // buttons sit low, over the darker part

    // left: 5s / 10s
    lv_obj_t *dwell = ui_box(s_bottom);
    lv_obj_set_size(dwell, 56, 32);
    lv_obj_set_style_radius(dwell, 16, 0);
    lv_obj_set_style_bg_color(dwell, COL_CARD, 0);
    lv_obj_set_style_bg_opa(dwell, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(dwell, COL_SW_OFF, LV_STATE_PRESSED);
    lv_obj_add_flag(dwell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(dwell, 6);   // 44 px tall touch target
    lv_obj_align(dwell, LV_ALIGN_LEFT_MID, 0, 0);
    s_dwell_label = ui_label(dwell, FONT_M_BOLD, COL_TEXT, "10s");
    lv_obj_center(s_dwell_label);
    lv_obj_add_event_cb(dwell, button_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)PhotoCmd::ToggleDwell);

    // centre: Prev 44, Pause/Play 52, Next 44, 14 px apart
    lv_obj_t *mid = ui_box(s_bottom);
    lv_obj_set_size(mid, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mid, 14, 0);
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 0);
    round_button(mid, 44, LV_SYMBOL_PREV, COL_CARD, LV_OPA_80, COL_TEXT, PhotoCmd::Prev);
    round_button(mid, 52, LV_SYMBOL_PAUSE, COL_CPU, LV_OPA_COVER, COL_ON_ACCENT, PhotoCmd::TogglePause, &s_play_label);
    round_button(mid, 44, LV_SYMBOL_NEXT, COL_CARD, LV_OPA_80, COL_TEXT, PhotoCmd::Next);

    // right: "3 / 24"
    s_ctrl_counter = ui_label(s_bottom, FONT_M_BOLD, COL_TEXT, "");
    lv_obj_align(s_ctrl_counter, LV_ALIGN_RIGHT_MID, 0, 0);
}

// ---------------- drawing ----------------

static void fade_exec(void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0); }

// Put image `front` above the other one and fade it in.
static void show_frame(uint8_t front, bool animate) {
    lv_image_cache_drop(&s_dsc[front]);   // same buffer, new pixels
    lv_obj_move_to_index(s_img[front], 1);   // above the other image, below the chips and bars
    set_hidden(s_img[front], false);
    lv_anim_delete(s_img[front], fade_exec);
    if (!animate) {
        lv_obj_set_style_opa(s_img[front], LV_OPA_COVER, 0);
        lv_obj_invalidate(s_img[front]);
        return;
    }
    lv_obj_set_style_opa(s_img[front], LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_img[front]);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, FADE_MS);
    lv_anim_start(&a);
}

static void draw_state() {
    const bool frame = s_ps.have_frame;
    char n[24] = "";
    if (s_ps.count) snprintf(n, sizeof n, "%u / %u", (unsigned)s_ps.index + 1, (unsigned)s_ps.count);
    lv_label_set_text(s_counter, n);
    lv_label_set_text(s_ctrl_counter, n);
    set_hidden(s_counter_chip, s_controls || !s_ps.count || !frame);
    lv_label_set_text(s_dwell_label, s_ps.dwell_s == 5 ? "5s" : "10s");
    lv_label_set_text(s_play_label, s_ps.paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);

    const char *msg = nullptr;   // centred message over the dark background
    switch (s_ps.status) {
    case PhotoStatus::Empty: msg = "No photos"; break;
    case PhotoStatus::NoMemory: msg = "No memory"; break;
    case PhotoStatus::NotConfigured: msg = "Pi 5 API not set"; break;
    case PhotoStatus::Offline: if (!frame) msg = "Offline"; break;
    default: break;
    }
    const bool empty = msg != nullptr && (s_ps.status != PhotoStatus::Offline || !frame);
    if (empty) lv_label_set_text(s_empty_text, msg);
    set_hidden(s_empty, !empty);
    if (empty) {   // "No photos" etc. hide whatever photo was there
        set_hidden(s_img[0], true);
        set_hidden(s_img[1], true);
    }
    set_hidden(s_spinner, !(s_ps.status == PhotoStatus::Loading && !frame) &&
                              !(s_ps.status == PhotoStatus::Idle && s_visible));
    set_hidden(s_offline, !(s_ps.status == PhotoStatus::Offline && frame));
}

// ---------------- public ----------------

void screen_photos_create(lv_obj_t *tile) {
    s_tile = tile;
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, tile_clicked_cb, LV_EVENT_CLICKED, nullptr);

    for (int k = 0; k < 2; k++) {
        s_dsc[k] = lv_image_dsc_t{};
        s_dsc[k].header.magic = LV_IMAGE_HEADER_MAGIC;
        s_dsc[k].header.cf = LV_COLOR_FORMAT_RGB565;
        s_dsc[k].header.w = PHOTO_W;
        s_dsc[k].header.h = PHOTO_H;
        s_dsc[k].header.stride = PHOTO_W * 2;
        s_dsc[k].data_size = PHOTO_W * PHOTO_H * 2;
        s_img[k] = lv_image_create(tile);   // source set once the service has buffers
        lv_obj_remove_flag(s_img[k], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(s_img[k], 0, 0);
        lv_obj_add_flag(s_img[k], LV_OBJ_FLAG_HIDDEN);
    }

    // empty / error message: gray photo glyph + one line
    s_empty = ui_box(tile);
    lv_obj_set_size(s_empty, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_empty, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_empty, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_empty, 8, 0);
    ui_label(s_empty, FONT_XL, COL_GRAY, LV_SYMBOL_IMAGE);
    s_empty_text = ui_label(s_empty, FONT_MD, COL_GRAY, "No photos");
    lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

    s_spinner = lv_spinner_create(tile);
    lv_obj_set_size(s_spinner, 40, 40);
    lv_spinner_set_anim_params(s_spinner, 1000, 270);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_spinner, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_spinner, COL_CPU, LV_PART_INDICATOR);
    lv_obj_remove_flag(s_spinner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_spinner);
    lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);

    s_counter_chip = chip(tile);
    s_counter = ui_label(s_counter_chip, FONT_S_BOLD, COL_TEXT, "");
    lv_obj_align(s_counter_chip, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_flag(s_counter_chip, LV_OBJ_FLAG_HIDDEN);

    s_offline = chip(tile);
    lv_obj_t *dot = ui_box(s_offline);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_STOPPED, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    ui_label(s_offline, FONT_S_BOLD, COL_TEXT, "Offline");
    lv_obj_align(s_offline, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_add_flag(s_offline, LV_OBJ_FLAG_HIDDEN);

    top_bar_create();
    bottom_bar_create();
    s_controls = true;   // force the first controls_show(false) to apply
    s_hide_timer = lv_timer_create(hide_timer_cb, CONTROLS_MS, nullptr);
    controls_show(false);
}

void screen_photos_set_pages(int count, int active) {
    s_pages = count < MAX_PAGES ? count : MAX_PAGES;
    for (int i = 0; i < MAX_PAGES; i++) {
        set_hidden(s_pills[i], i >= s_pages);
        lv_obj_set_width(s_pills[i], i == active ? 20 : 8);
        lv_obj_set_style_bg_color(s_pills[i], i == active ? COL_CPU : COL_PILL_OFF, 0);
    }
}

void screen_photos_set_visible(bool visible) {
    if (visible == s_visible) return;
    s_visible = visible;
    photo_service_command(visible ? PhotoCmd::Show : PhotoCmd::Hide);
    if (!visible) controls_show(false);
    draw_state();
}

void screen_photos_poll() {
    if (state_version(StateGroup::Net) != s_net_ver) {
        s_net_ver = state_version(StateGroup::Net);
        lv_obj_set_style_text_color(s_wifi, state_get_net().wifi_connected ? COL_OK : COL_GRAY, 0);
    }
    if (state_version(StateGroup::Photos) == s_ver) return;
    s_ver = state_version(StateGroup::Photos);
    s_ps = state_get_photos();

    for (int k = 0; k < 2; k++) {
        if (s_ps.frames[k] && s_dsc[k].data != (const uint8_t *)s_ps.frames[k]) {
            s_dsc[k].data = (const uint8_t *)s_ps.frames[k];   // fixed PSRAM buffer: set once
            lv_image_set_src(s_img[k], &s_dsc[k]);
        }
    }
    if (s_ps.have_frame && s_ps.frame_seq != s_seq_shown) {
        const bool first = s_seq_shown == 0;
        s_seq_shown = s_ps.frame_seq;
        show_frame(s_ps.front, !first && s_visible);
    }
    draw_state();
}
