#pragma once

#include <lvgl.h>

class ClockWidget
{
public:
    static void init(lv_obj_t *parent);

private:
    static lv_obj_t *time_label;
    static lv_timer_t *timer;

    static void update_time(lv_timer_t *timer);
};