#include "light_icon.h"

lv_obj_t *LightIcon::create(lv_obj_t *parent, const char *title)
{
    lv_obj_t *light_icon_content = lv_obj_create(parent);
    lv_obj_set_size(light_icon_content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(light_icon_content, LV_FLEX_FLOW_COLUMN);

    btn = lv_btn_create(light_icon_content);
    lv_obj_set_size(btn, 80, 80);
    // lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);

    icon = lv_label_create(btn);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_center(icon);

    lv_obj_t *title_footer = lv_label_create(light_icon_content);
    lv_label_set_text_fmt(title_footer, title);
    lv_obj_align(title_footer, LV_ALIGN_BOTTOM_MID, 0, 0);

    return btn;
}

void LightIcon::turn_off()
{
    lv_obj_set_style_bg_color(
        btn,
        lv_palette_main(LV_PALETTE_YELLOW),
        0);

    lv_obj_set_style_text_color(
        icon,
        lv_color_white(),
        0);
};

void LightIcon::turn_on()
{
    lv_obj_set_style_bg_color(
        btn,
        lv_palette_main(LV_PALETTE_YELLOW),
        0);

    lv_obj_set_style_text_color(
        icon,
        lv_color_white(),
        0);
};