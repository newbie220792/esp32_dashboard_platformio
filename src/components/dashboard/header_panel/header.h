/**
 * @file header.h
 *
 */

#pragma once

#ifndef LV_HEADER_H
#define LV_HEADER_H

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

    lv_obj_t *create_header_panel(lv_obj_t *parent);

        /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_HEADER_H*/
