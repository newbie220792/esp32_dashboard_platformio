#pragma once

#include <lvgl.h>

class WeatherPage
{
public:
    void init(lv_obj_t *parent);

    lv_obj_t *getContent();

private:
    lv_obj_t *content;
    lv_obj_t *title;
};
