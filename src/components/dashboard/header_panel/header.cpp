#include "header.h"
#include <config/colors.h>

HeaderPanel::HeaderPanel(lv_obj_t *parent)
{
    header_panel = lv_obj_create(parent);
    lv_obj_set_style_text_font(header_panel, &lv_font_montserrat_10, 0);

    /* Time */
    time_label = lv_label_create(header_panel);
    lv_label_set_text(time_label, "14:25:00");
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_set_align(time_label, LV_ALIGN_LEFT_MID);

    /* Wifi */
    wifi_label = lv_label_create(header_panel);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(Color::WIFI_ACTIVE), 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, 0, 0);
};

void HeaderPanel::updateCurrentTime(const char *currentTime)
{
    lv_label_set_text(time_label, currentTime);
};

lv_obj_t *HeaderPanel::getPanel()
{
    return this->header_panel;
};

void HeaderPanel::updateWifiStatus(bool isConnected)
{
    if (isConnected)
    {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(
            wifi_label,
            lv_color_hex(Color::WIFI_ACTIVE),
            0);
    }
    else
    {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(
            wifi_label,
            lv_color_hex(Color::WIFI_INACTIVE),
            0);
    }
};