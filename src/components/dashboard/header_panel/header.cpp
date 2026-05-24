#include "header.h"

lv_obj_t *time_label;
lv_obj_t *wifi_label;

lv_obj_t *create_header_panel(lv_obj_t *parent)
{
    lv_obj_t *header = lv_obj_create(parent);

    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_GRID_CONTENT);

    // lv_obj_set_style_bg_color(
    //     header,
    //     lv_color_hex(0x1A1A1A),
    //     0);

    // lv_obj_set_style_border_width(header, 0, 0);
    // lv_obj_set_style_radius(header, 10, 0);
    lv_obj_set_style_text_font(
        header,
        &lv_font_montserrat_10,
        0);

    /* grid */
    static lv_coord_t col_dsc[] = {
        LV_GRID_FR(1),
        LV_GRID_CONTENT,
        LV_GRID_TEMPLATE_LAST};

    static lv_coord_t row_dsc[] = {
        LV_GRID_FR(1),
        LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(header, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(header, col_dsc, row_dsc);

    /* Time */
    time_label = lv_label_create(header);
    lv_label_set_text(time_label, "14:25:00");

    lv_obj_set_style_text_color(
        time_label,
        lv_color_white(),
        0);

    lv_obj_set_grid_cell(
        time_label,
        LV_GRID_ALIGN_START, 0, 1,
        LV_GRID_ALIGN_CENTER, 0, 1);

    /* Wifi */
    wifi_label = lv_label_create(header);

    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_size(wifi_label, LV_PCT(100), LV_PCT(100));

    lv_obj_set_style_text_color(
        wifi_label,
        lv_color_hex(0x00E676),
        0);

    lv_obj_set_grid_cell(
        wifi_label,
        LV_GRID_ALIGN_END, 1, 1,
        LV_GRID_ALIGN_CENTER, 0, 1);

    return header;
}

void ui_update_time(const char *time_str)
{
    lv_label_set_text(time_label, time_str);
}

void ui_update_wifi_status(bool connected)
{
    if (connected)
    {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " Connected");
        lv_obj_set_style_text_color(
            wifi_label,
            lv_color_hex(0x00E676),
            0);
    }
    else
    {
        lv_label_set_text(wifi_label, "Disconnected");
        lv_obj_set_style_text_color(
            wifi_label,
            lv_palette_main(LV_PALETTE_RED),
            0);
    }
}