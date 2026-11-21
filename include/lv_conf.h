/**
 * LVGL 9.2 configuration for the ESP32 dashboard.
 * Only non-default options are listed; everything else falls back to
 * lvgl/src/lv_conf_internal.h defaults.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Color / memory */
#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_MEM_SIZE (64U * 1024U)

/* HAL */
#define LV_DEF_REFR_PERIOD 20
#define LV_DPI_DEF 130

/* LVGL is only touched from the LVGL task (see main.cpp), so no OS layer */
#define LV_USE_OS LV_OS_NONE

/* Rendering */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE

/* Logging / debug */
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets used: obj, label, arc, tileview (others left at defaults) */
#define LV_USE_ARC 1
#define LV_USE_CHART 0
#define LV_USE_LABEL 1
#define LV_USE_TILEVIEW 1
#define LV_USE_CALENDAR 0
#define LV_USE_KEYBOARD 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABLE 0
#define LV_USE_TEXTAREA 1 /* required by some defaults; cheap */

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 0

/* No demos / examples */
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0

#endif /* LV_CONF_H */
