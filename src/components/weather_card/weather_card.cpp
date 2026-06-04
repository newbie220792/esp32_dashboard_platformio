#include "weather_card.h"
#include <core/lv_custom_symbol.h>
#include <string>
#include "assets/images/upload_icon.h"
#include "assets/images/suny.h"
#include "assets/images/cloudy.h"
#include "assets/images/rainy.h"

void WeatherCard::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, 120, 80);
    lv_obj_set_style_radius(content, 16, 0);
    lv_obj_set_style_bg_color(
        content,
        lv_color_hex(0x1E293B),
        0);

    // Weather icon
    weather_icon = lv_img_create(content);
    lv_img_set_src(weather_icon, &upload_icon);

    lv_obj_set_style_text_color(
        weather_icon,
        lv_color_hex(0x94A3B8),
        0);
    lv_obj_center(weather_icon);

    // Temperature label
    label_temp = lv_label_create(content);
    lv_label_set_text(label_temp, temperature ? (std::to_string(temperature) + "°C").c_str() : "--°C");

    lv_obj_set_style_text_font(
        label_temp,
        &lv_font_montserrat_14,
        0);
    lv_obj_align(label_temp, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Humidity label
    label_humidity = lv_label_create(content);
    lv_label_set_text(label_humidity, humidity ? (std::to_string(humidity) + "%").c_str() : "--%");

    lv_obj_set_style_text_font(
        label_humidity,
        &lv_font_montserrat_14,
        0);
    lv_obj_align(label_humidity, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
};

void WeatherCard::updateWeatherData(const char *weather = "Sunny")
{
    // update weather icon based on weather condition
    if (strcmp(weather, "Sunny") == 0)
    {
        lv_img_set_src(weather_icon, &suny);
    }
    else if (strcmp(weather, "Cloudy") == 0)
    {
        lv_img_set_src(weather_icon, &cloudy);
    }
    else if (strcmp(weather, "Rainy") == 0)
    {
        lv_img_set_src(weather_icon, &rainy);
    }
    else
    {
        lv_img_set_src(weather_icon, &upload_icon);
    }
};

void WeatherCard::updateTemperature(uint32_t temperature)
{
    lv_label_set_text(label_temp, (std::to_string(temperature) + "°C").c_str());
};

void WeatherCard::updateHumidity(uint32_t humidity)
{
    lv_label_set_text(label_humidity, (std::to_string(humidity) + "%").c_str());
};