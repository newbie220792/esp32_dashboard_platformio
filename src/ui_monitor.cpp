#include "ui_monitor.h"

#include "app_config.h"
#include "ui_theme.h"

#define TEMP_MAX_C   85
#define TEMP_HOT_C   70
#define CHART_POINTS 30     // one sample every 2 s -> 60 s window
#define CHART_PERIOD 2000

struct Gauge {
    lv_obj_t *arc;
    lv_obj_t *value;
    lv_obj_t *caption;
    lv_color_t color;
};

static Gauge g_cpu, g_ram, g_temp;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_series;
static lv_obj_t *s_avg;

static float s_last_cpu = 0;
static uint32_t s_last_rx_ms = 0;
static bool s_have_data = false;

static bool data_fresh() {
    return s_have_data && (lv_tick_elaps(s_last_rx_ms) < MQTT_STALE_MS);
}

static void gauge_create(Gauge &g, lv_obj_t *row, lv_color_t color, const char *caption) {
    lv_obj_t *card = ui_card(row, 6);
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);

    g.color = color;
    g.arc = lv_arc_create(card);
    lv_obj_set_size(g.arc, 80, 80);
    lv_arc_set_rotation(g.arc, 135);
    lv_arc_set_bg_angles(g.arc, 0, 270);
    lv_arc_set_range(g.arc, 0, 100);
    lv_arc_set_value(g.arc, 0);
    lv_obj_remove_style(g.arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(g.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(g.arc, 0, 0);
    lv_obj_set_style_arc_width(g.arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g.arc, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g.arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g.arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_INDICATOR);

    g.value = ui_label(g.arc, FONT_L, COL_MUTED, "--");
    lv_obj_center(g.value);

    g.caption = ui_label(card, FONT_S, COL_MUTED, caption);
}

static void gauge_set(Gauge &g, int32_t arc_value, const char *text) {
    lv_arc_set_value(g.arc, arc_value);
    lv_label_set_text(g.value, text);
    lv_obj_set_style_text_color(g.value, COL_TEXT, 0);
}

static void update_avg() {
    int32_t *ys = lv_chart_get_y_array(s_chart, s_series);
    int32_t sum = 0, n = 0;
    for (uint32_t i = 0; i < CHART_POINTS; i++) {
        if (ys[i] != LV_CHART_POINT_NONE) {
            sum += ys[i];
            n++;
        }
    }
    if (n) lv_label_set_text_fmt(s_avg, "avg %d%%", (int)((sum + n / 2) / n));
    else lv_label_set_text(s_avg, "avg --");
}

// Sample on a fixed clock so the chart really spans 60 s even if the
// publisher's rate drifts; gaps (stale data) are drawn as breaks.
static void chart_timer_cb(lv_timer_t *) {
    int32_t v = data_fresh() ? (int32_t)(s_last_cpu + 0.5f) : LV_CHART_POINT_NONE;
    lv_chart_set_next_value(s_chart, s_series, v);
    update_avg();

    if (s_have_data && !data_fresh()) {
        lv_obj_set_style_text_color(g_cpu.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_ram.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_temp.value, COL_MUTED, 0);
    }
}

void ui_monitor_create(lv_obj_t *tile) {
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, UI_GAP, 0);

    // Gauges row
    lv_obj_t *row = ui_box(tile);
    lv_obj_set_size(row, LV_PCT(100), 112);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    gauge_create(g_cpu, row, COL_CPU, "CPU");
    gauge_create(g_ram, row, COL_RAM, "RAM");
    gauge_create(g_temp, row, COL_TEMP, "TEMP");
    lv_arc_set_range(g_temp.arc, 0, TEMP_MAX_C);

    // CPU history card
    lv_obj_t *card = ui_card(tile, 6);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_pad_hor(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 2, 0);

    lv_obj_t *hdr = ui_box(card);
    lv_obj_set_size(hdr, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_t *title = ui_label(hdr, FONT_S, COL_MUTED, "CPU - 60s");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
    s_avg = ui_label(hdr, FONT_S, COL_CPU, "avg --");
    lv_obj_align(s_avg, LV_ALIGN_RIGHT_MID, 0, 0);

    s_chart = lv_chart_create(card);
    lv_obj_set_width(s_chart, LV_PCT(100));
    lv_obj_set_flex_grow(s_chart, 1);
    lv_obj_remove_flag(s_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_radius(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);
    lv_obj_set_style_pad_top(s_chart, 3, 0);     // room for the 2px line at 100%
    lv_obj_set_style_pad_bottom(s_chart, 3, 0);
    lv_obj_set_style_line_color(s_chart, COL_TRACK, LV_PART_MAIN);   // grid
    lv_obj_set_style_line_width(s_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(s_chart, true, LV_PART_ITEMS);
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);         // no points
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(s_chart, 3, 0);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(s_chart, CHART_POINTS);
    s_series = lv_chart_add_series(s_chart, COL_CPU, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(s_chart, s_series, LV_CHART_POINT_NONE);

    lv_timer_create(chart_timer_cb, CHART_PERIOD, nullptr);
}

void ui_monitor_update(const PiStats &s) {
    char buf[24];
    s_last_cpu = s.cpu;
    s_last_rx_ms = lv_tick_get();
    s_have_data = true;

    snprintf(buf, sizeof buf, "%d%%", (int)(s.cpu + 0.5f));
    gauge_set(g_cpu, (int32_t)(s.cpu + 0.5f), buf);

    snprintf(buf, sizeof buf, "%d%%", (int)(s.ram + 0.5f));
    gauge_set(g_ram, (int32_t)(s.ram + 0.5f), buf);
    if (s.ram_total > 0)
        lv_label_set_text_fmt(g_ram.caption, "RAM %.1f/%gG", s.ram_used, s.ram_total);

    snprintf(buf, sizeof buf, "%d\xC2\xB0", (int)(s.temp + 0.5f));   // degree sign
    gauge_set(g_temp, (int32_t)(s.temp + 0.5f), buf);
    lv_obj_set_style_arc_color(g_temp.arc, s.temp > TEMP_HOT_C ? COL_HOT : COL_TEMP, LV_PART_INDICATOR);
}
