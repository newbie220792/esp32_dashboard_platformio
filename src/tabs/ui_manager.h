#pragma once

#include <lvgl.h>
#include <map>
#include "tabs/home_tab/home_tab.h"
#include "components/header_panel/header.h"
#include "tabs/pi_monitoring_tab/pi_monitoring_tab.h"
#include "tabs/light_control_tab/light_control_tab.h"
#include "disp_size_t.h"

class UIManager
{
public:
    void init();

private:
    HeaderPanel header;
    PiMonitoringTab piMonitoringTab;
    LightControlTab lightControlTab;
};