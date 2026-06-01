#include "home_page.h"
#include <Arduino.h>
#include <pages/ui_manager.h>

static void navigate_pi_async(void *arg)
{
    UIManager::navigate(ScreenId::PI_MONITORING_PAGE);
}

void btn_navigate_pi(lv_event_t *e)
{
    lv_async_call(navigate_pi_async, nullptr);
}

void HomePage::init(lv_obj_t *parent)
{
    content = lv_obj_create(parent);
    lv_obj_set_size(content, LV_PCT(100), lv_pct(100));

    lv_obj_t *title_lable = lv_label_create(content);
    lv_label_set_text(title_lable, title);

    lv_obj_t *btn_navigator = lv_btn_create(content);
    lv_obj_add_event_cb(btn_navigator, btn_navigate_pi, LV_EVENT_PRESSED, NULL);
    lv_obj_align(btn_navigator, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *labelNavigator = lv_label_create(btn_navigator);
    lv_label_set_text(labelNavigator, "To Home");
    lv_obj_center(labelNavigator);

    // lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
};

const char *HomePage::getTitle()
{
    return "Home Page";
};

void HomePage::destroy()
{
    // if (content)
    // {
    //     lv_obj_del(content);
    //     content = nullptr;
    //     return;
    // }
    Serial.println("destroy home page");
};