#include "pi_monitoring_tab.h"
#include "core/app_queues.h"
#include <config/message_event.h>
#include <config/topic.h>
#include <Arduino.h>
#include "tabs/ui_manager.h"
#include <config/ui_constants.h>
#include <ArduinoJson.h>
#include <config/room.h>

static lv_obj_t *cpu_chart;
static lv_obj_t *mem_chart;

static lv_chart_series_t *cpu_ser;
static lv_chart_series_t *mem_ser;

static lv_obj_t *cpu_label;
static lv_obj_t *mem_label;

static lv_obj_t *temp_label;
static lv_obj_t *temp_bar;

static lv_style_t style_indic;
static bool style_init_done = false;

lv_obj_t *create_chart(
    lv_obj_t **chart,
    lv_chart_series_t **ser,
    lv_obj_t **label,
    lv_obj_t *parent,
    const char *name,
    lv_color_t color)
{
    if (*ser != NULL)
    {
        Serial.println("Series is not null, skip create chart");
        return nullptr;
    }
    lv_obj_t *cont = lv_obj_create(parent);

    lv_obj_set_size(cont, LV_PCT(45), 90);

    lv_obj_set_style_bg_color(
        cont,
        lv_color_hex(0x1A1A1A),
        0);

    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_set_style_radius(cont, 10, 0);

    lv_obj_t *txt = lv_label_create(cont);
    lv_obj_set_style_text_font(
        txt,
        &lv_font_montserrat_10,
        0);

    lv_label_set_text(txt, name);

    lv_obj_set_style_text_color(
        txt,
        lv_color_white(),
        0);

    lv_obj_align(txt, LV_ALIGN_TOP_LEFT, 0, 0);

    *label = lv_label_create(cont);

    lv_label_set_text(*label, "0%");

    lv_obj_set_style_text_color(
        *label,
        color,
        0);

    lv_obj_set_style_text_font(
        *label,
        &lv_font_montserrat_10,
        0);

    lv_obj_align(*label, LV_ALIGN_TOP_RIGHT, 0, 0);

    *chart = lv_chart_create(cont);

    lv_obj_set_size(*chart, LV_PCT(100), 50);

    lv_obj_align(*chart, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_chart_set_type(
        *chart,
        LV_CHART_TYPE_BAR);

    lv_chart_set_range(
        *chart,
        LV_CHART_AXIS_PRIMARY_Y,
        0,
        100);

    lv_chart_set_point_count(*chart, 20);

    lv_obj_set_style_bg_opa(*chart, LV_OPA_TRANSP, 0);

    lv_obj_set_style_line_width(*chart, 0, LV_PART_MAIN);

    lv_obj_set_style_border_width(*chart, 0, 0);

    *ser = lv_chart_add_series(
        *chart,
        color,
        LV_CHART_AXIS_PRIMARY_Y);

    for (int i = 0; i < 20; i++)
    {
        lv_chart_set_next_value(
            *chart,
            *ser,
            0);
    }
    return cont;
}

lv_obj_t *create_temp_gauge(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);

    lv_obj_set_size(cont, LV_PCT(100), 50);

    lv_obj_set_style_bg_color(
        cont,
        lv_color_hex(0x1A1A1A),
        0);

    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_set_style_radius(cont, 18, 0);

    lv_obj_t *txt = lv_label_create(cont);

    lv_label_set_text(txt, "TEMP");

    lv_obj_set_style_text_color(
        txt,
        lv_color_white(),
        0);

    lv_obj_align(txt, LV_ALIGN_LEFT_MID, 15, 0);

    //  create temp bar
    if (!style_init_done)
    {

        lv_style_init(&style_indic);
        lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
        lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
        lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);
        style_init_done = true;
    }
    temp_bar = lv_bar_create(cont);
    lv_obj_add_style(temp_bar, &style_indic, LV_PART_INDICATOR);
    lv_obj_set_size(temp_bar, 200, 20);
    lv_obj_align(temp_bar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(temp_bar, -20, 80);

    // temp_label
    temp_label = lv_label_create(cont);

    lv_label_set_text(temp_label, "0.0");

    lv_obj_set_style_text_color(
        temp_label,
        lv_color_white(),
        0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 0);
    return cont;
}

lv_obj_t *PiMonitoringTab::init(lv_obj_t *parent)
{
    Serial.println("Init PiMonitoringTab");
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *cpu_cont = create_chart(
        &cpu_chart,
        &cpu_ser,
        &cpu_label,
        content,
        "CPU",
        lv_color_hex(0x00D0FF));
    lv_obj_align(cpu_cont, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *mem_cont = create_chart(
        &mem_chart,
        &mem_ser,
        &mem_label,
        content,
        "MEMORY",
        lv_color_hex(0x00E676));
    lv_obj_align(mem_cont, LV_ALIGN_TOP_LEFT, UI::SCREEN_WIDTH * 0.5 - 10, 0);

    lv_obj_t *temp_cont = create_temp_gauge(content);
    lv_obj_align(temp_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollbar_mode(temp_cont, LV_SCROLLBAR_MODE_OFF);

    return content;
}

void PiMonitoringTab::ui_update_cpu(int value)
{
    if (cpu_chart == NULL)
        return;
    if (cpu_ser == NULL)
        return;
    if (cpu_label == NULL)
        return;

    lv_chart_set_next_value(
        cpu_chart,
        cpu_ser,
        value);

    lv_label_set_text_fmt(
        cpu_label,
        "%d%%",
        value);
}

void PiMonitoringTab::ui_update_mem(int value)
{
    if (mem_chart == NULL)
        return;
    if (mem_ser == NULL)
        return;
    if (mem_label == NULL)
        return;

    lv_chart_set_next_value(
        mem_chart,
        mem_ser,
        value);

    lv_label_set_text_fmt(
        mem_label,
        "%d%%",
        value);
}

void PiMonitoringTab::ui_update_temp(float value)
{
    if (temp_bar == NULL)
        return;
    if (temp_label == NULL)
        return;

    lv_bar_set_value(temp_bar, value, LV_ANIM_ON);

    char buf[32];

    int t_int = (int)value;
    int t_dec = (int)(value * 10) % 10;

    snprintf(buf,
             sizeof(buf),
             "%d.%d\xC2\xB0"
             "C",
             t_int,
             t_dec);
    lv_label_set_text(temp_label, buf);
}

const char *PiMonitoringTab::getTitle()
{
    return "Pi Monitoring Tab";
};