#pragma once

#include <lvgl.h>

class Card
{
public:
    lv_obj_t *create(lv_obj_t *parent, const char *title);
};