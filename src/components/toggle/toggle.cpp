

#include "toggle.h"

void Toggle::create(lv_obj_t *parent, const char *text, lv_event_cb_t event_cb)
{

    lv_obj_t *btn2 = lv_btn_create(parent);
    lv_obj_add_event_cb(btn2, event_cb, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    lv_obj_t *label = lv_label_create(btn2);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}