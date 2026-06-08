#pragma once

#include <lvgl.h>
#include <stdio.h>
#include "tabs/base_tab.h"

class PiMonitoringTab : public BaseTab
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
    const char *getTitle() override;

    static void ui_update_cpu(int value);
    static void ui_update_mem(int value);
    static void ui_update_temp(float value);

private:
    // static lv_obj_t *cpu_chart;
    // static lv_obj_t *mem_chart;

    // static lv_chart_series_t *cpu_ser;
    // static lv_chart_series_t *mem_ser;

    // static lv_obj_t *cpu_label;
    // static lv_obj_t *mem_label;
    // static lv_obj_t *temp_label;
    // static lv_obj_t *temp_bar;

    lv_obj_t *create_temp_gauge(lv_obj_t *parent);
    // bool style_init_done;
    // static lv_style_t style_indic;
};