#pragma once
#include <lvgl.h>

// Palette
#define COL_BG        lv_color_hex(0x0B0F14)
#define COL_CARD      lv_color_hex(0x151B23)
#define COL_TRACK     lv_color_hex(0x263140)
#define COL_TEXT      lv_color_hex(0xE6EDF3)
#define COL_MUTED     lv_color_hex(0x8B98A8)
#define COL_CPU       lv_color_hex(0x3DD6C6)
#define COL_RAM       lv_color_hex(0x9AA8FF)
#define COL_TEMP      lv_color_hex(0xF5A524)
#define COL_HOT       lv_color_hex(0xF25F5C)
#define COL_OK        lv_color_hex(0x3DDC84)
#define COL_GRAY      lv_color_hex(0x6B7788)
#define COL_PILL_OFF  lv_color_hex(0x3A4656)
#define COL_ON_ACCENT lv_color_hex(0x0B0F14)   // dark text on #3DD6C6
#define COL_STOPPED   lv_color_hex(0xFF6B6B)   // service stopped ring
#define COL_SCROLLBAR lv_color_hex(0x3A4656)
#define COL_PANEL     lv_color_hex(0x1D2733)   // home: switch-cluster panel
#define COL_SW_ON     lv_color_hex(0xF5A524)
#define COL_SW_OFF    lv_color_hex(0x2A3646)
#define COL_SW_OFF_FG lv_color_hex(0xA3AEBB)

// Metrics (480x272)
#define UI_PAD        10
#define UI_GAP        8
#define UI_RADIUS     12
#define UI_STATUS_H   28

// Fonts
#define FONT_S   (&lv_font_montserrat_12)
#define FONT_M   (&lv_font_montserrat_14)
#define FONT_MD  (&lv_font_montserrat_16)
#define FONT_L   (&lv_font_montserrat_20)
#define FONT_XL  (&lv_font_montserrat_48)
// LVGL ships no bold Montserrat. Point these at generated 600-weight fonts
// (lv_font_conv) to get real bold; until then they are the regular sizes.
#define FONT_S_BOLD  FONT_S
#define FONT_M_BOLD  FONT_M
#define FONT_MD_BOLD FONT_MD
#define FONT_L_BOLD  FONT_L

// Plain container: no theme styles, transparent, not scrollable/clickable.
lv_obj_t *ui_box(lv_obj_t *parent);
// Flat card: #151B23, radius 10, no border/shadow.
lv_obj_t *ui_card(lv_obj_t *parent, int32_t pad);
// Thin 3px scrollbar (#3A4656) on the right edge of a scrollable container.
void ui_style_scrollbar(lv_obj_t *obj);
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
