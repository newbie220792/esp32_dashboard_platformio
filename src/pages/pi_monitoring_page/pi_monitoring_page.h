#pragma once

#include <lvgl.h>
#include <stdio.h>
#include "pages/base_page.h"

class PiMonitoringPage : public BasePage
{
public:
    void init(lv_obj_t *parent) override;
    const char *getTitle() override;
    void destroy() override;

    void ui_update_cpu(int value);
    void ui_update_mem(int value);
    void ui_update_temp(float value);

private:
    lv_obj_t *cpu_chart;
    lv_obj_t *mem_chart;

    lv_chart_series_t *cpu_ser;
    lv_chart_series_t *mem_ser;

    lv_obj_t *cpu_label;
    lv_obj_t *mem_label;

    lv_obj_t *temp_label;
    lv_obj_t *temp_bar;

    lv_obj_t *create_temp_gauge(lv_obj_t *parent);
    bool style_init_done;
};