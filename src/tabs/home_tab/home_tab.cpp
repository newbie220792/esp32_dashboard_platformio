#include "home_tab.h"
#include <Arduino.h>

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
    lv_obj_set_size(left_col_content, LV_PCT(100), LV_PCT(100));

    return left_col_content;
}

lv_obj_t *HomePage::initial_right_col(lv_obj_t *content)
{
    lv_obj_t *right_col_content = lv_obj_create(content);
    lv_obj_set_size(right_col_content, LV_PCT(100), LV_PCT(100));

    // weather card
    lv_obj_t *weather_card = card.create(right_col_content, NULL);

    return right_col_content;
}