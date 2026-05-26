#include "ui_manager.h"
#include <config/ui_constants.h>

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

    // initial all of the pages
    homePage.init(content_panel);
    weatherPage.init(content_panel);

    // add pages
    pages[(int)(ScreenId::HOME_PAGE)] = homePage.getContent();
    pages[(int)ScreenId::WEATHER_PAGE] = weatherPage.getContent();

    // testing loading page
    loadingPage(ScreenId::HOME_PAGE);
    delay(3);
    loadingPage(ScreenId::WEATHER_PAGE);
    delay(3);
    loadingPage(ScreenId::PI_MONITORING_PAGE);
};

void UIManager::loadingPage(ScreenId screenId)
{
    if (screenId == currentPage)
        return;
    lv_obj_add_flag(
        pages[(int)currentPage],
        LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(
        pages[(int)screenId],
        LV_OBJ_FLAG_HIDDEN);

    currentPage = screenId;
};