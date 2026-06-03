#include "ui_manager.h"
#include <config/ui_constants.h>
#include <core/lv_custom_symbol.h>

BasePage *pages[(int)ScreenId::COUNT];
ScreenId currentPage = ScreenId::NONE;

static lv_obj_t *content_panel;
static lv_obj_t *main_panel;
static HomePage homePage;

void UIManager::init()
{
    // main_pannel
    main_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(main_panel, 0, 0);
    lv_obj_set_style_border_width(main_panel, 0, 0);

    // header_pannel
    HeaderPanel headerPanel;
    headerPanel.init(main_panel);

    // initial tabview
    lv_obj_t *tabview = lv_tabview_create(main_panel, LV_DIR_TOP, UI::HEADER_HEIGHT);

    lv_obj_t *home_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);
    lv_obj_t *content = homePage.init(home_tab);

    lv_obj_t *pi_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
    content = piMonitoringPage.init(pi_tab);

    lv_obj_t *weather_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_CLOUD);
    content = weatherPage.init(weather_tab);

    lv_obj_t *light_control_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_LIGHT);
    content = lightControlPage.init(light_control_tab);

    lv_obj_scroll_to_view_recursive(content, LV_ANIM_ON);
};