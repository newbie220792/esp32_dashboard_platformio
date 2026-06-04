#pragma once

#include <lvgl.h>
#include <stdio.h>
#include "pages/base_page.h"

class PiMonitoringPage : public BasePage
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
     const char *getTitle() override;

     static void ui_update_cpu(int value);
     static void ui_update_mem(int value);
     static void ui_update_temp(float value);

 private:
};