/**
 * @file ui_dashboard.h
 *
 */

#pragma once

#ifndef LV_UI_DASHBOARD_H
#define LV_UI_DASHBOARD_H

#ifdef __cplusplus
extern "C"
{
#endif

// #define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <stdio.h>

    /**********************
     * GLOBAL PROTOTYPES
     **********************/

    void ui_dashboard_create(void);

    void ui_update_cpu(int value);
    void ui_update_mem(int value);
    void ui_update_temp(float value);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_WIDGETS_H*/
