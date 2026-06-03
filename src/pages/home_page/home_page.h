#pragma once

#ifndef LV_HOME_PAGE_H
#define LV_HOME_PAGE_H

#include <lvgl.h>
#include "pages/base_page.h"

class HomePage : public BasePage
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
     const char *getTitle() override;

 private:
};

#endif