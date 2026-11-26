#pragma once
// One home switch (~140x58): vector icon, name, ON/OFF. ON = amber #F5A524 with
// dark text; OFF = #2A3646. Pending (waiting for .../state) dims to 40% with
// "..."; MQTT down dims to 50%.
#include <lvgl.h>

#include "core/models.h"

struct SwitchTile {
    lv_obj_t *btn, *icon, *name, *state;
};

// Always adds a 1/3-width slot to `row`. Placeholders (!sw.present) stay empty
// and return false; otherwise `on_click(user)` is wired and it returns true.
bool switch_tile_create(SwitchTile &t, lv_obj_t *row, const HomeSwitch &sw, lv_event_cb_t on_click, void *user);
void switch_tile_apply(SwitchTile &t, const HomeSwitch &sw, bool mqtt_ok);
