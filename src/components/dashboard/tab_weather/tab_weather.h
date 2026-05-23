/**
 * @file tab_weather.h
 *
 */

#pragma once

#ifndef LV_TAB_WEATHER_H
#define LV_TAB_WEATHER_H

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

    void lv_weather_create(lv_obj_t *parent);
    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_TAB_WEATHER_H*/
