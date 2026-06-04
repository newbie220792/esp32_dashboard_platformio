
#pragma once
#include "lvgl.h"

class LightIcon
{
public:
    lv_obj_t *create(lv_obj_t *parent, const char *title);
    void turn_on();
    void turn_off();

private:
    lv_obj_t *btn;
    lv_obj_t *icon;
    int index;
};