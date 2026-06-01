#include "header.h"
#include <config/colors.h>
#include <config/ui_constants.h>

static lv_obj_t *time_label;
static lv_obj_t *wifi_label;

void HeaderPanel::init(lv_obj_t *parent)
{
    header_panel = lv_obj_create(parent);
    lv_obj_set_style_text_font(header_panel, &lv_font_montserrat_12, 0);
    lv_obj_set_size(header_panel, LV_PCT(100), UI::HEADER_HEIGHT);
    lv_obj_align(header_panel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scrollbar_mode(header_panel, LV_SCROLLBAR_MODE_OFF);

    /* Time */
    time_label = lv_label_create(header_panel);
    lv_label_set_text(time_label, "14:25:00");
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_set_align(time_label, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_pad_all(time_label, 0, 0);

    /* Wifi */
    wifi_label = lv_label_create(header_panel);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(Color::WIFI_INACTIVE), 0);
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, 0, 0);
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