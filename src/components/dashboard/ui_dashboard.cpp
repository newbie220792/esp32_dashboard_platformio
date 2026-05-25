/**
 * @file lv_ui_dashboard.cpp
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "ui_dashboard.h"
#include "tab_profile/tab_profile.h"
#include "tab_weather/tab_weather.h"
#include "tab_pi_monitor/pi_monitor.h"
#include "header_panel/header.h"
#include "disp_size_t.h"
#include <config/ui_constants.h>

// #ifndef LV_USE_DEMO_WIDGETS
//     #define LV_USE_DEMO_WIDGETS 1
// #endif

#if LV_MEM_CUSTOM == 0 && LV_MEM_SIZE < (38ul * 1024ul)
#error Insufficient memory for lv_demo_widgets. Please set LV_MEM_SIZE to at least 38KB (38ul * 1024ul).  48KB is recommended.
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
// static void analytics_create(lv_obj_t *parent);
// static void shop_create(lv_obj_t *parent);
// static void color_changer_create(lv_obj_t *parent);
// static void weather_create(lv_obj_t *parent);

/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

static lv_obj_t *tv;
static lv_obj_t *calendar;
static lv_style_t style_text_muted;
static lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

static lv_obj_t *meter1;
static lv_obj_t *meter2;
static lv_obj_t *meter3;

static lv_obj_t *chart1;
static lv_obj_t *chart2;
static lv_obj_t *chart3;

static lv_chart_series_t *ser1;
static lv_chart_series_t *ser2;
static lv_chart_series_t *ser3;
static lv_chart_series_t *ser4;

static const lv_font_t *font_large;
static const lv_font_t *font_normal;

static uint32_t session_desktop = 1000;
static uint32_t session_tablet = 1000;
static uint32_t session_mobile = 1000;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_dashboard_create(void)
{
        if (LV_HOR_RES <= 320)
                disp_size = DISP_SMALL;
        else if (LV_HOR_RES < 720)
                disp_size = DISP_MEDIUM;
        else
                disp_size = DISP_LARGE;

        font_large = LV_FONT_DEFAULT;
        font_normal = LV_FONT_DEFAULT;

        lv_coord_t tab_h;
        if (disp_size == DISP_LARGE)
        {
                tab_h = 70;
#if LV_FONT_MONTSERRAT_24
                font_large = &lv_font_montserrat_24;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_24 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_16
                font_normal = &lv_font_montserrat_16;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
        }
        else if (disp_size == DISP_MEDIUM)
        {
                tab_h = 45;
#if LV_FONT_MONTSERRAT_20
                font_large = &lv_font_montserrat_20;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_20 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_14
                font_normal = &lv_font_montserrat_14;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_14 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
        }
        else
        { /* disp_size == DISP_SMALL */
                tab_h = 45;
#if LV_FONT_MONTSERRAT_18
                font_large = &lv_font_montserrat_18;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_12
                font_normal = &lv_font_montserrat_12;
#else
                LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
#endif
        }

#if LV_USE_THEME_DEFAULT
        lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK,
                              font_normal);
#endif

        lv_style_init(&style_text_muted);
        lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

        lv_style_init(&style_title);
        lv_style_set_text_font(&style_title, font_large);

        lv_style_init(&style_icon);
        lv_style_set_text_color(&style_icon, lv_theme_get_color_primary(NULL));
        lv_style_set_text_font(&style_icon, font_large);

        lv_style_init(&style_bullet);
        lv_style_set_border_width(&style_bullet, 0);
        lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

        lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);

        // main_pannel
        lv_obj_t *main_panel = lv_obj_create(lv_scr_act());
        lv_obj_set_size(main_panel, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_pad_all(main_panel, 0, 0);
        lv_obj_set_style_border_width(main_panel, 0, 0);

        // header_pannel
        HeaderPanel *header = new HeaderPanel(main_panel);

        // content_pannel
        lv_obj_t *content_panel = lv_obj_create(main_panel);
        lv_obj_set_size(content_panel, UI::SCREEN_WIDTH, UI::SCREEN_HEIGHT - UI::HEADER_HEIGHT);

        lv_obj_set_style_border_width(content_panel, 0, 0);

        lv_obj_set_style_pad_all(content_panel, 0, 0);
        lv_obj_align(content_panel, LV_ALIGN_TOP_MID, 0, UI::HEADER_HEIGHT);

        // TAB_VIEW
        // tv = lv_tabview_create(content_pannel, LV_DIR_TOP, 30);
        // lv_obj_set_size(tv, LV_PCT(100), LV_PCT(100));

        // lv_obj_t *t1 = lv_tabview_add_tab(tv, "PI Monitoring");
        // lv_obj_t *t2 = lv_tabview_add_tab(tv, "Profile");
        // lv_obj_t *t3 = lv_tabview_add_tab(tv, "Weather");

        pi_monitor_tab_create(content_panel);
        // profile_create(content_panel, disp_size);
        // lv_weather_create(t3);
}