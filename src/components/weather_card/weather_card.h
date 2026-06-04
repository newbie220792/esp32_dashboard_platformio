#pragma once

#include "lvgl.h"

class WeatherCard
{
public:
    void init(lv_obj_t *parent);

    static void updateWeatherData(const char *weather);
    static void updateTemperature(uint32_t temperature);
    static void updateHumidity(uint32_t humidity);

private:
    lv_obj_t *content;
    uint32_t temperature;
    uint32_t humidity;

    static lv_obj_t *weather_icon;
    static lv_obj_t *label_temp;
    static lv_obj_t *label_humidity;
};