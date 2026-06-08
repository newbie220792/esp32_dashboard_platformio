#include "ui_manager.h"
#include <config/ui_constants.h>
#include <core/lv_custom_symbol.h>

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

    // content_pannel
    content_panel = lv_obj_create(main_panel);
    lv_obj_set_size(content_panel, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT - UI::HEADER_HEIGHT);
    lv_obj_set_style_border_width(content_panel, 0, 0);
    lv_obj_set_style_pad_all(content_panel, 0, 0);
    lv_obj_align(content_panel, LV_ALIGN_TOP_MID, 0, UI::HEADER_HEIGHT);

    lv_color_t color = lv_color_make(0xF4, 0x43, 0x36);
    lv_obj_set_style_bg_color(content_panel, lv_color_darken(color, LV_OPA_30), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(content_panel, LV_OPA_COVER, LV_STATE_PRESSED);

    // initial tabview
    lv_obj_t *tabview = lv_tabview_create(content_panel, LV_DIR_TOP, UI::HEADER_HEIGHT);

    lv_obj_t *home_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);
    lv_obj_t *content = homePage.init(home_tab);

    lv_obj_t *pi_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_EYE_OPEN);
    content = piMonitoringTab.init(pi_tab);

    // lv_obj_t *light_control_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
    // content = lightControlTab.init(light_control_tab);

    lv_obj_scroll_to_view_recursive(content, LV_ANIM_ON);
};