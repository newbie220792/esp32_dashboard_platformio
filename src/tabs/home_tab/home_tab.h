#pragma once

#ifndef LV_HOME_TAB_H
#define LV_HOME_TAB_H

#include <lvgl.h>
#include "tabs/base_tab.h"
#include "components/card_component/card.h"
#include <components/weather_card/weather_card.h>

class HomePage : public BaseTab
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
    const char *getTitle() override;

private:
    Card card;
    WeatherCard weatherCard;
};

#endif