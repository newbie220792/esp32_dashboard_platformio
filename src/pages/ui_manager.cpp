#include "ui_manager.h"
#include <config/ui_constants.h>

BasePage *pages[(int)ScreenId::COUNT];
ScreenId currentPage = ScreenId::NONE;
static lv_obj_t *content_panel;
static lv_obj_t *main_panel;
static HomePage homePage;

void UIManager::init()
{
    // main_pannel
    // lv_layer
    main_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(main_panel, 0, 0);
    lv_obj_set_style_border_width(main_panel, 0, 0);

    // header_pannel
    HeaderPanel headerPanel;
    headerPanel.init(main_panel);

    // content_pannel
    content_panel = lv_obj_create(main_panel);
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

    Serial.printf(
        "Navigate to screen: %d, currentPage:%d\n",
        (int)screenId,
        (int)currentPage);

    // destroy old panel
    if (content_panel)
    {
        BasePage *old_page = pages[(int)screenId];
        if (old_page == nullptr)
        {
            Serial.println("new_page NULL");
            return;
        }
        old_page->destroy();
        delete old_page;
        lv_obj_del(content_panel);
        content_panel = nullptr;
    }

    // create new panel
    content_panel = lv_obj_create(main_panel);

    lv_obj_set_size(
        content_panel,
        UI::SCREEN_WIDTH,
        UI::SCREEN_HEIGHT - UI::HEADER_HEIGHT);

    lv_obj_set_style_border_width(content_panel, 0, 0);
    lv_obj_set_style_pad_all(content_panel, 0, 0);

    lv_obj_align(
        content_panel,
        LV_ALIGN_TOP_MID,
        0,
        UI::HEADER_HEIGHT);

    BasePage *new_page = pages[(int)screenId];

    if (new_page == nullptr)
    {
        Serial.println("new_page NULL");
        return;
    }

    new_page->init(content_panel);

    currentPage = screenId;
};