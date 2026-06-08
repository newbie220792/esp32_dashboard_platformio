#include "weather_card.h"
#include <core/lv_custom_symbol.h>
#include <string>
#include "assets/images/upload_icon.h"
#include "assets/images/suny.h"
#include "assets/images/cloudy.h"
#include "assets/images/rainy.h"

static lv_obj_t *weather_icon;
static lv_obj_t *label_temp;
static lv_obj_t *label_humidity;

void WeatherCard::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    // lv_obj_set_style_radius(content, 16, 0);
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
    if (weather_icon == NULL)
    {
        Serial.println("Weather icon is null, cannot update weather data");
        return;
    }
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

void WeatherCard::updateTemperature(char *temperature)
{
    if (label_temp == NULL)
    {
        Serial.println("Temperature label is null, cannot update temperature");
        return;
    }
    std::string temp_text = std::string(temperature) + "°C";
    lv_label_set_text(label_temp, temp_text.c_str());
};

void WeatherCard::updateHumidity(char *humidity)
{
    if (label_humidity == NULL)
    {
        Serial.println("Humidity label is null, cannot update humidity");
        return;
    }
    std::string humidity_text = std::string(humidity) + "%";
    lv_label_set_text(label_humidity, humidity_text.c_str());
};