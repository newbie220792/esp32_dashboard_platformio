#pragma once

#include <lvgl.h>
#include <stdio.h>

class PiMonitoringPage
{
public:
    void init(lv_obj_t *parent);

    lv_obj_t *getContent();
    static void ui_update_cpu(int value);
    static void ui_update_mem(int value);
    static void ui_update_temp(float value);

private:
    lv_obj_t *content;
    lv_obj_t *title;
};