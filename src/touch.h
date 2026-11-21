#pragma once
#include <lvgl.h>

// GT911 (TouchLib) as an LVGL pointer input device. Call after display_init().
// The read callback runs inside lv_timer_handler(), i.e. on the LVGL task.
lv_indev_t *touch_init();
