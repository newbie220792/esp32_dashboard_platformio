#pragma once

#include <lvgl.h>
#include <Arduino.h>

class BasePage
{
public:
    virtual void init(lv_obj_t *parent) = 0;
    virtual void destroy() = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual const char *getTitle() = 0;

    lv_obj_t *getContent()
    {
        return content;
    }

protected:
    lv_obj_t *content = nullptr;
    const char *title = nullptr;
};