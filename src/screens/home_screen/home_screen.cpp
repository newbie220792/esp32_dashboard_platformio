#include "home_screen.h"

HomeScreen::HomeScreen(lv_obj_t* parent) {

    panel = lv_obj_create(parent);

    lv_obj_set_size(panel, LV_PCT(100), lv_pct(100));

}