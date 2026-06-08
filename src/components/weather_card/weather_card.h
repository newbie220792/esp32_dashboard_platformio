#pragma once

#include "lvgl.h"
#include <Arduino.h>

class WeatherCard
{
public:
    void init(lv_obj_t *parent);

    static void updateWeatherData(const char *weather);
    static void updateTemperature(char *temperature);
    static void updateHumidity(char *humidity);

private:
    lv_obj_t *content;
    float temperature;
    float humidity;
};