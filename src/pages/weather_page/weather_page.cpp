#include "weather_page.h"

static lv_obj_t *label_temp;
static lv_obj_t *label_status;
static lv_obj_t *label_humidity;
static lv_obj_t *label_wind;
static lv_obj_t *label_city;

void WeatherPage::init(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent,
                              lv_color_hex(0x0F172A), 0);

    // Temperature
    label_temp = lv_label_create(parent);
    lv_label_set_text(label_temp, "31°C");

    lv_obj_set_style_text_font(
        label_temp,
        &lv_font_montserrat_14,
        0);

    lv_obj_set_style_text_color(
        label_temp,
        lv_color_white(),
        0);

    lv_obj_align(label_temp,
                 LV_ALIGN_TOP_MID,
                 0,
                 30);

    // Weather Status
    label_status = lv_label_create(parent);
    lv_label_set_text(label_status, "Cloudy");

    lv_obj_set_style_text_font(
        label_status,
        &lv_font_montserrat_14,
        0);

    lv_obj_set_style_text_color(
        label_status,
        lv_color_hex(0xCBD5E1),
        0);

    lv_obj_align_to(label_status,
                    label_temp,
                    LV_ALIGN_OUT_BOTTOM_MID,
                    0,
                    10);

    // City
    label_city = lv_label_create(parent);
    lv_label_set_text(label_city, "Binh Duong");

    lv_obj_set_style_text_font(
        label_city,
        &lv_font_montserrat_14,
        0);

    lv_obj_set_style_text_color(
        label_city,
        lv_color_hex(0x94A3B8),
        0);

    lv_obj_align_to(label_city,
                    label_status,
                    LV_ALIGN_OUT_BOTTOM_MID,
                    0,
                    8);

    // Humidity card
    lv_obj_t *hum_card = lv_obj_create(parent);
    lv_obj_set_size(hum_card, 120, 80);

    lv_obj_align(hum_card,
                 LV_ALIGN_BOTTOM_LEFT,
                 20,
                 -20);

    lv_obj_set_style_radius(hum_card, 16, 0);
    lv_obj_set_style_bg_color(
        hum_card,
        lv_color_hex(0x1E293B),
        0);

    label_humidity = lv_label_create(hum_card);
    lv_label_set_text(label_humidity,
                      "Humidity\n72%");

    lv_obj_center(label_humidity);

    // Wind card
    lv_obj_t *wind_card = lv_obj_create(parent);
    lv_obj_set_size(wind_card, 120, 80);

    lv_obj_align(wind_card,
                 LV_ALIGN_BOTTOM_RIGHT,
                 -20,
                 -20);

    lv_obj_set_style_radius(wind_card, 16, 0);
    lv_obj_set_style_bg_color(
        wind_card,
        lv_color_hex(0x1E293B),
        0);

    label_wind = lv_label_create(wind_card);
    lv_label_set_text(label_wind,
                      "Wind\n12 km/h");

    lv_obj_center(label_wind);

    // lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
}

const char *WeatherPage::getTitle()
{
    return "Weather Page";
};

void WeatherPage::destroy()
{
    content = nullptr;
    title = nullptr;
    Serial.println("destroy weather page");
}
