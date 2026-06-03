#include "home_page.h"
#include <Arduino.h>
#include <pages/ui_manager.h>

lv_obj_t *HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), lv_pct(100));

    lv_obj_t *title_lable = lv_label_create(content);
    lv_label_set_text(title_lable, title);
    return content;
};

const char *HomePage::getTitle()
{
    return "Home Page";
};