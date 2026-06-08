#pragma once

#include <lvgl.h>
#if LV_USE_BTN
class Toggle
{
public:
    static void create(lv_obj_t *parent, const char *text = "Button", lv_event_cb_t event_cb = nullptr);
    void addHandle(lv_event_cb_t event_cb);
};
#endif