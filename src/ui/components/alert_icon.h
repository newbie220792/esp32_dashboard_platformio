#pragma once
// Alert severity icon: shape *and* colour differ, so it reads without colour.
//   critical  filled red circle #FF6B6B with "!"
//   warning   filled amber triangle #F5A524 with "!"
//   info      rounded square outlined in #9AA8FF with "i"
#include <lvgl.h>

#include "core/models.h"

lv_obj_t *alert_icon_create(lv_obj_t *parent, int32_t size);
void alert_icon_set(lv_obj_t *icon, AlertLevel level);
