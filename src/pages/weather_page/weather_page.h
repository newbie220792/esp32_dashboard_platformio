#pragma once

#include <lvgl.h>
#include "pages/base_page.h"
class WeatherPage : public BasePage
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
    const char *getTitle() override;

private:
};
