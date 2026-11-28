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
/* LVGL heap in PSRAM: 64 KB internal was full after ui_init (task 9: 99 %, the
 * first lv_timer_handler then hung in LV_ASSERT_MALLOC = black screen). PSRAM
 * also keeps internal RAM for Wi-Fi/TLS. Falls back to internal RAM without PSRAM. */
#define LV_MEM_SIZE (192U * 1024U)
#define LV_MEM_POOL_INCLUDE <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) \
    (heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) ?: heap_caps_malloc(size, MALLOC_CAP_8BIT))

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
/* Only the sizes the UI uses (flash is 4 MB). lv_conf_internal.h turns 14 on by default. */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets used: obj, label, arc, tileview, spinner, bar, qrcode */
#define LV_USE_ARC 1
#define LV_USE_CHART 0
#define LV_USE_LABEL 1
#define LV_USE_TILEVIEW 1
#define LV_USE_SPINNER 1
#define LV_USE_BAR 1
#define LV_USE_QRCODE 1   /* Wi-Fi setup screen */
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
