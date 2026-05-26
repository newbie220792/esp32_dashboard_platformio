/**
 * @file pi_monitoring.h
 *
 */

#pragma once

#ifndef LV_PI_MONITORING_H
#define LV_PI_MONITORING_H

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

    void pi_monitor_tab_create_t(lv_obj_t *parent);

    void ui_update_cpu_t(int value);
    void ui_update_mem_t(int value);
    void ui_update_temp_t(float value);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PI_MONITORING_H*/
