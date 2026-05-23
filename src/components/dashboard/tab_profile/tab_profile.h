/**
 * @file profile.h
 *
 */

#pragma once

#ifndef LV_PROFILE_H
#define LV_PROFILE_H

#ifdef __cplusplus
extern "C"
{
#endif

// #define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <stdio.h>
#include "disp_size_t.h"

    /**********************
     * GLOBAL PROTOTYPES
     **********************/

    void profile_create(lv_obj_t *parent, disp_size_t disp_size);
    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PROFILE_H*/
