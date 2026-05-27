#pragma once

#include <lvgl.h>
#include "disp_size_t.h"

class ProfilePage
{
public:
    void init(lv_obj_t *parent);
    lv_obj_t *getContent();

private:
    lv_obj_t *content;
    lv_obj_t *title;
    disp_size_t disp_size;
};