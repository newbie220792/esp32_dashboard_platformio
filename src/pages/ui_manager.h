#pragma once

#include <lvgl.h>
#include <config/screen_id_t.h>
#include <map>
#include "pages/home_page/home_page.h"
#include "pages/weather_page/weather_page.h"
#include "components/header_panel/header.h"
#include "disp_size_t.h"

class UIManager
{
public:
    void init();
    void loadingPage(ScreenId screenId);

private:
    HeaderPanel header;
    HomePage homePage;
    WeatherPage weatherPage;

    lv_obj_t *pages[(int)ScreenId::COUNT];
    ScreenId currentPage;
};