#pragma once
#include <lvgl.h>
#include "models.h"

// Tile 3: home switches (room -> panel -> 3 switches). LVGL task only.
//
// A tap publishes home/<room>/<panel>/<id>/set with the opposite state and
// dims the switch; its colour changes only when .../state comes back. If no
// reply arrives within SWITCH_PENDING_MS the switch reverts to its old look.
void ui_home_create(lv_obj_t *tile);
void ui_home_set_config(HomeConfig *cfg);   // takes ownership (delete'd later)
void ui_home_apply_state(const SwitchState &st);
void ui_home_set_mqtt(bool connected);
