#pragma once

#include "tabs/base_tab.h"
#include "components/card_component/card.h"
#include "components/light_icon/light_icon.h"

class LightControlPage : public BaseTab
{
public:
    lv_obj_t *init(lv_obj_t *parent) override;
    const char *getTitle() override;

private:
    Card card;
    LightIcon lightIcon;
};