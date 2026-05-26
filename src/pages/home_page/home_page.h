#pragma once

#ifndef LV_HOME_PAGE_H
#define LV_HOME_PAGE_H

#include <lvgl.h>

class HomePage
{
public:
    void init(lv_obj_t *parent);

    lv_obj_t *getContent();

private:
    lv_obj_t *content;
    lv_obj_t *title;
};

#endif