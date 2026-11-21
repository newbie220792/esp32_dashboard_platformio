#include "ui_weather.h"

#include "ui_theme.h"
#include "weather_icon.h"

#define DEG "\xC2\xB0"

static lv_obj_t *s_icon, *s_temp, *s_unit, *s_cond, *s_feels, *s_hum, *s_wind;

struct HourCard {
    lv_obj_t *time, *icon, *temp;
};
static HourCard s_hours[1 + WEATHER_HOURS];   // [0] = Now

static lv_obj_t *stat_block(lv_obj_t *parent, const char *title, lv_obj_t **value_out) {
    lv_obj_t *b = ui_box(parent);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ui_label(b, FONT_S, COL_MUTED, title);
    *value_out = ui_label(b, FONT_M, COL_TEXT, "--");
    return b;
}

static void hour_card_create(HourCard &h, lv_obj_t *row, bool highlight) {
    lv_obj_t *card = ui_card(row, 4);
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
    h.icon = weather_icon_create(card, 30);
    h.temp = ui_label(card, FONT_M, fg, "--" DEG);
}

void ui_weather_create(lv_obj_t *tile) {
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, UI_GAP, 0);

    // ---- current conditions -------------------------------------------
    lv_obj_t *big = ui_card(tile, UI_PAD);
    lv_obj_set_size(big, LV_PCT(100), 104);

    s_icon = weather_icon_create(big, 64);
    lv_obj_align(s_icon, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *mid = ui_box(big);
    lv_obj_set_size(mid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(mid, LV_ALIGN_LEFT_MID, 64 + 12, 0);

    lv_obj_t *trow = ui_box(mid);
    lv_obj_set_size(trow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(trow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    s_temp = ui_label(trow, FONT_XL, COL_TEXT, "--");
    lv_obj_set_style_text_line_space(s_temp, 0, 0);
    s_unit = ui_label(trow, FONT_L, COL_MUTED, DEG "C");
    lv_obj_set_style_pad_top(s_unit, 6, 0);
    lv_obj_set_style_pad_left(s_unit, 2, 0);

    s_cond = ui_label(mid, FONT_M, COL_TEXT, "Loading...");
    lv_label_set_long_mode(s_cond, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_cond, 130);
    s_feels = ui_label(mid, FONT_S, COL_MUTED, "Feels like --" DEG);

    lv_obj_t *right = ui_box(big);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(right, 8, 0);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
    stat_block(right, "Humidity", &s_hum);
    stat_block(right, "Wind", &s_wind);

    // ---- now + next 4 hours ---------------------------------------------
    lv_obj_t *row = ui_box(tile);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    for (int i = 0; i < 1 + WEATHER_HOURS; i++) hour_card_create(s_hours[i], row, i == 0);
}

void ui_weather_update(const WeatherData &w) {
    weather_icon_set(s_icon, w.code, w.is_day);
    lv_label_set_text_fmt(s_temp, "%d", (int)lroundf(w.temp));
    lv_label_set_text(s_cond, weather_code_text(w.code));
    lv_label_set_text_fmt(s_feels, "Feels like %d" DEG, (int)lroundf(w.feels_like));
    lv_label_set_text_fmt(s_hum, "%d%%", (int)lroundf(w.humidity));
    lv_label_set_text_fmt(s_wind, "%d km/h", (int)lroundf(w.wind_kmh));

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
