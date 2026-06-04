#include "home_page.h"
#include <Arduino.h>
#include <pages/ui_manager.h>

lv_obj_t *HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *left_col = initial_left_col(content);
    lv_obj_t *right_col = initial_right_col(content);

    return content;
};

const char *HomePage::getTitle()
{
    return "Home Page";
};

lv_obj_t *HomePage::initial_left_col(lv_obj_t *content)
{
    lv_obj_t *left_col_content = lv_obj_create(content);
    // weather card
    lv_obj_t *weather_card = card.create(left_col_content, "Weather");

    return left_col_content;
}

lv_obj_t *HomePage::initial_right_col(lv_obj_t *content)
{
    lv_obj_t *right_col_content = lv_obj_create(content);

    return right_col_content;
}