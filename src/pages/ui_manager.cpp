#include "ui_manager.h"
#include <config/ui_constants.h>

BasePage *pages[(int)ScreenId::COUNT];
ScreenId currentPage = ScreenId::NONE;

void UIManager::init()
{
    // main_pannel
    lv_obj_t *main_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(main_panel, 0, 0);
    lv_obj_set_style_border_width(main_panel, 0, 0);

    // header_pannel
    HeaderPanel headerPanel;
    headerPanel.init(main_panel);

    // content_pannel
    lv_obj_t *content_panel = lv_obj_create(main_panel);
    lv_obj_set_size(content_panel, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT - UI::HEADER_HEIGHT);
    lv_obj_set_style_border_width(content_panel, 0, 0);
    lv_obj_set_style_pad_all(content_panel, 0, 0);
    lv_obj_align(content_panel, LV_ALIGN_TOP_MID, 0, UI::HEADER_HEIGHT);

    // add pages
    pages[(int)(ScreenId::HOME_PAGE)] = &homePage;
    pages[(int)ScreenId::WEATHER_PAGE] = &weatherPage;
    pages[(int)ScreenId::PI_MONITORING_PAGE] = &piMonitoringPage;

    // testing loading page
    navigate(ScreenId::PI_MONITORING_PAGE);
};

void UIManager::navigate(ScreenId screenId)
{
    if (screenId == currentPage)
        return;

    if (currentPage)
    {
        // re-create page
        BasePage *new_page = pages[(int)screenId];
        new_page->init(NULL);

        // destroy old page
        BasePage *old_page = pages[(int)currentPage];
        old_page->destroy();
        currentPage = screenId;
    }
    else
    {
        Serial.println("CurrentPage is not created");
    }
};