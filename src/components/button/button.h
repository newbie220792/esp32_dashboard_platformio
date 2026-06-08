#pragma once

#include <lvgl.h>

class Button
{
public:
    static void create(lv_obj_t *parent, const char *text = "Button", lv_event_cb_t event_cb = nullptr);
    static void addHandle(lv_event_cb_t event_cb);

private:
    static lv_obj_t *btn1;
};