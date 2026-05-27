#include "home_page.h"

void HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), lv_pct(100));

    title = lv_label_create(content);
    lv_label_set_text(title, "Home page");

    lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *HomePage::getContent()
{
    return content;
};