#pragma once

#include <lvgl.h>
#include <config/screen_id_t.h>
#include <map>
#include "pages/home_page/home_page.h"
#include "pages/weather_page/weather_page.h"
#include "components/header_panel/header.h"
#include "pages/pi_monitoring_page/pi_monitoring_page.h"
#include "pages/light_control_page/light_control_page.h"
#include "disp_size_t.h"

class UIManager
{
public:
    void init();

private:
    HeaderPanel header;
    PiMonitoringPage piMonitoringPage;
    LightControlPage lightControlPage;
};