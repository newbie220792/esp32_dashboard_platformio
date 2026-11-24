#include "ui_weather.h"

#include "ui_theme.h"
#include "weather_icon.h"

#define DEG "\xC2\xB0"

#define MAIN_CARD_H  116
#define MAIN_ICON    84
#define HOUR_ICON    28
#define STATS_W      150

static lv_obj_t *s_icon, *s_temp, *s_unit, *s_cond, *s_feels, *s_hum, *s_wind, *s_rain, *s_stale;
static bool s_have_data = false;
static bool s_failing = false;      // last fetch failed
static uint32_t s_last_ok_ms = 0;   // lv_tick of the last good fetch

struct HourCard {
    lv_obj_t *time, *icon, *temp;
};
static HourCard s_hours[1 + WEATHER_HOURS];   // [0] = Now

// One line of the right-hand column: "Humidity ........ 78%".
static void stat_row(lv_obj_t *parent, const char *title, lv_obj_t **value_out) {
    lv_obj_t *r = ui_box(parent);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_t *t = ui_label(r, FONT_S, COL_MUTED, title);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 0, 0);
    *value_out = ui_label(r, FONT_MD_BOLD, COL_TEXT, "--");
    lv_obj_align(*value_out, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void hour_card_create(HourCard &h, lv_obj_t *row, bool highlight) {
    lv_obj_t *card = ui_card(row, 6);
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_color_t fg = COL_TEXT, sub = COL_MUTED;
    if (highlight) {
        lv_obj_set_style_bg_color(card, COL_CPU, 0);
        fg = sub = COL_ON_ACCENT;
    }
    h.time = ui_label(card, FONT_S, sub, highlight ? "Now" : "--:--");
    h.icon = weather_icon_create(card, HOUR_ICON);
    h.temp = ui_label(card, FONT_MD_BOLD, fg, "--" DEG);
}

static const char *err_text(WeatherErr e) {
    switch (e) {
    case WeatherErr::NoNetwork:   return "No network";
    case WeatherErr::Timeout:     return "Timeout";
    case WeatherErr::ServerError: return "Server error";
    case WeatherErr::BadData:     return "Bad response";
    }
    return "";
}

static void stale_refresh() {
    if (!s_have_data || !s_failing) {
        lv_obj_add_flag(s_stale, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const uint32_t min = lv_tick_elaps(s_last_ok_ms) / 60000;
    if (min == 0) lv_label_set_text(s_stale, "Updated just now");
    else lv_label_set_text_fmt(s_stale, "Updated %lu min ago", (unsigned long)min);
    lv_obj_remove_flag(s_stale, LV_OBJ_FLAG_HIDDEN);
}

static void stale_timer_cb(lv_timer_t *) { stale_refresh(); }

void ui_weather_create(lv_obj_t *tile) {
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, UI_GAP, 0);

    // ---- current conditions -------------------------------------------
    lv_obj_t *big = ui_card(tile, UI_PAD);
    lv_obj_set_size(big, LV_PCT(100), MAIN_CARD_H);
    lv_obj_set_style_pad_hor(big, 14, 0);

    s_icon = weather_icon_create(big, MAIN_ICON);
    lv_obj_align(s_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *mid = ui_box(big);
    lv_obj_set_size(mid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(mid, LV_ALIGN_LEFT_MID, MAIN_ICON + 14, 0);

    lv_obj_t *trow = ui_box(mid);
    lv_obj_set_size(trow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(trow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    s_temp = ui_label(trow, FONT_XL, COL_TEXT, "--");
    lv_obj_set_style_text_line_space(s_temp, 0, 0);
    s_unit = ui_label(trow, FONT_L, COL_MUTED, DEG "C");
    lv_obj_set_style_pad_top(s_unit, 8, 0);
    lv_obj_set_style_pad_left(s_unit, 2, 0);

    s_cond = ui_label(mid, FONT_MD_BOLD, COL_TEXT, "Loading...");
    lv_label_set_long_mode(s_cond, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_cond, 160);
    s_feels = ui_label(mid, FONT_S, COL_MUTED, "Feels like --" DEG);

    lv_obj_t *right = ui_box(big);
    lv_obj_set_size(right, STATS_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 6, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    stat_row(right, "Humidity", &s_hum);
    stat_row(right, "Wind", &s_wind);
    stat_row(right, "Rain chance", &s_rain);
    s_stale = ui_label(right, FONT_S, COL_TEMP, "");   // only while fetches fail
    lv_obj_set_style_text_align(s_stale, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(s_stale, LV_PCT(100));
    lv_obj_add_flag(s_stale, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(stale_timer_cb, 30000, nullptr);

    // ---- now + next 4 hours ---------------------------------------------
    lv_obj_t *row = ui_box(tile);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    for (int i = 0; i < 1 + WEATHER_HOURS; i++) hour_card_create(s_hours[i], row, i == 0);
}

void ui_weather_update(const WeatherData &w) {
    s_have_data = true;
    s_failing = false;
    s_last_ok_ms = lv_tick_get();
    stale_refresh();
    weather_icon_set(s_icon, w.code, w.is_day);
    lv_label_set_text_fmt(s_temp, "%d", (int)lroundf(w.temp));
    lv_label_set_text(s_cond, weather_code_text(w.code));
    lv_label_set_text_fmt(s_feels, "Feels like %d" DEG, (int)lroundf(w.feels_like));
    lv_label_set_text_fmt(s_hum, "%d%%", (int)lroundf(w.humidity));
    lv_label_set_text_fmt(s_wind, "%d km/h", (int)lroundf(w.wind_kmh));
    if (w.rain_chance >= 0) lv_label_set_text_fmt(s_rain, "%d%%", (int)w.rain_chance);
    else lv_label_set_text(s_rain, "--");

    weather_icon_set(s_hours[0].icon, w.code, w.is_day);
    lv_label_set_text_fmt(s_hours[0].temp, "%d" DEG, (int)lroundf(w.temp));

    for (int i = 0; i < WEATHER_HOURS; i++) {
        HourCard &h = s_hours[i + 1];
        if (i < w.hour_count) {
            const WeatherHour &wh = w.hours[i];
            lv_label_set_text_fmt(h.time, "%02d:00", wh.hour);
            weather_icon_set(h.icon, wh.code, wh.is_day);
            lv_label_set_text_fmt(h.temp, "%d" DEG, (int)lroundf(wh.temp));
        } else {
            lv_label_set_text(h.time, "--:--");
            lv_label_set_text(h.temp, "--" DEG);
        }
    }
}

void ui_weather_set_error(WeatherErr e) {
    s_failing = true;
    if (!s_have_data) {
        lv_label_set_text(s_cond, "Weather unavailable");
        lv_label_set_text(s_feels, err_text(e));
    }
    stale_refresh();
}
