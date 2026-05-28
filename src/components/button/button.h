#pragma once

#include <lvgl.h>

class Button
{
public:
    void create(lv_obj_t *parent, lv_event_cb_t event_cb = nullptr);
    void addHandle(lv_event_cb_t event_cb);

private:
    lv_obj_t *btn1;
};