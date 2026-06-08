#pragma once

#include <lvgl.h>
#include <Arduino.h>

class BaseTab
{
public:
    virtual lv_obj_t *init(lv_obj_t *parent) = 0;
    virtual const char *getTitle() = 0;

    lv_obj_t *getContent()
    {
        return content;
    }

protected:
    lv_obj_t *content = nullptr;
    const char *title = nullptr;
};