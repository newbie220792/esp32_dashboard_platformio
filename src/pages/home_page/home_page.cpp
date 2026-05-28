#include "home_page.h"
#include <Arduino.h>

void HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), lv_pct(100));

    lv_obj_t *title_lable = lv_label_create(content);
    lv_label_set_text(title_lable, title);

    // lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
};

const char *HomePage::getTitle()
{
    return "Home Page";
};

// void HomePage::destroy()
// {
//     if (content)
//     {
//         lv_obj_del(content);
//         content = nullptr;
//         return;
//     }
//     Serial.println("Content is invalid");
// };