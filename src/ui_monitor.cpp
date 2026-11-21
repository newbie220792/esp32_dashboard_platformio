#include "ui_monitor.h"

#include "app_config.h"
#include "ui_theme.h"

#define TEMP_MAX_C     85
#define TEMP_HOT_C     70
#define ARC_SIZE       88
#define ARC_WIDTH      10
#define STALE_CHECK_MS 1000

struct Gauge {
    lv_obj_t *arc;
    lv_obj_t *value;
    lv_obj_t *caption;
};

static Gauge g_cpu, g_ram, g_temp;
static uint32_t s_last_rx_ms = 0;
static bool s_have_data = false;

static void gauge_create(Gauge &g, lv_obj_t *row, lv_color_t color, const char *caption) {
    lv_obj_t *card = ui_card(row, 6);
    lv_obj_set_style_pad_hor(card, 4, 0);   // ~97px card -> ~89px inner, fits the 88px arc
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    // Center the arc + caption group vertically and horizontally in the card.
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    g.arc = lv_arc_create(card);
    lv_obj_set_size(g.arc, ARC_SIZE, ARC_SIZE);
    lv_arc_set_rotation(g.arc, 135);
    lv_arc_set_bg_angles(g.arc, 0, 270);
    lv_arc_set_range(g.arc, 0, 100);
    lv_arc_set_value(g.arc, 0);
    lv_obj_remove_style(g.arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(g.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(g.arc, 0, 0);
    lv_obj_set_style_arc_width(g.arc, ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g.arc, COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g.arc, ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g.arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g.arc, true, LV_PART_INDICATOR);

    g.value = ui_label(g.arc, FONT_GAUGE, COL_MUTED, "--");
    lv_obj_center(g.value);

    g.caption = ui_label(card, FONT_S, COL_MUTED, caption);
}

static void gauge_set(Gauge &g, int32_t arc_value, const char *text) {
    lv_arc_set_value(g.arc, arc_value);
    lv_label_set_text(g.value, text);
    lv_obj_set_style_text_color(g.value, COL_TEXT, 0);
}

// Gray out the values when pi5/stats has gone quiet.
static void stale_timer_cb(lv_timer_t *) {
    if (s_have_data && lv_tick_elaps(s_last_rx_ms) >= MQTT_STALE_MS) {
        lv_obj_set_style_text_color(g_cpu.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_ram.value, COL_MUTED, 0);
        lv_obj_set_style_text_color(g_temp.value, COL_MUTED, 0);
    }
}

void ui_monitor_create(lv_obj_t *tile) {
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);

    // Gauges row fills the whole tile (~304 x 204 inside the 8px padding).
    lv_obj_t *row = ui_box(tile);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP, 0);
    gauge_create(g_cpu, row, COL_CPU, "CPU");
    gauge_create(g_ram, row, COL_RAM, "RAM");
    gauge_create(g_temp, row, COL_TEMP, "TEMP");
    lv_arc_set_range(g_temp.arc, 0, TEMP_MAX_C);

    lv_timer_create(stale_timer_cb, STALE_CHECK_MS, nullptr);
}

void ui_monitor_update(const PiStats &s) {
    char buf[24];
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
