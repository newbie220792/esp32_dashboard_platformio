#pragma once

#include <lvgl.h>
#include "pages/base_page.h"
class WeatherPage : public BasePage
{
public:
    void init(lv_obj_t *parent) override;
    const char *getTitle() override;
    void destroy() override;
    void show() override;
    void hide() override;

private:
};
