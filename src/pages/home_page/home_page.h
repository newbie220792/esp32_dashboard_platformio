#pragma once

#ifndef LV_HOME_PAGE_H
#define LV_HOME_PAGE_H

#include <lvgl.h>
#include "pages/base_page.h"
#include "components/card_component/card.h"

class HomePage : public BasePage
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
    const char *getTitle() override;

private:
    Card card;
    lv_obj_t *initial_left_col(lv_obj_t *content);
    lv_obj_t *initial_right_col(lv_obj_t *content);
};

#endif