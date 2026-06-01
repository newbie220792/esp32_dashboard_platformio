#include "light_control_page.h"

void LightControlPage::init(lv_obj_t *parent)
{
    // initial content
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);

    // initial room card
    lv_obj_t *second_floor_card = card.create(content, "Second Floor");
    lv_obj_set_flex_flow(second_floor_card, LV_FLEX_FLOW_ROW);

    lightIcon.create(second_floor_card, "Btn 1");
    lightIcon.create(second_floor_card, "Btn 2");
    lightIcon.create(second_floor_card, "Btn 3");
};

const char *LightControlPage::getTitle()
{
    return "Light Control Page";
};

void LightControlPage::destroy()
{
    Serial.println("destroy home page");
};