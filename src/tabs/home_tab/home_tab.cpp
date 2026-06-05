#include "home_tab.h"
#include <Arduino.h>

lv_obj_t *HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);

    // Weather card
    weatherCard.init(content);
    return content;
};

const char *HomePage::getTitle()
{
    return "Home Page";
};