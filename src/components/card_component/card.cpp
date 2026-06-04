#include "card.h"

lv_obj_t *Card::create(lv_obj_t *parent, const char *title)
{
    lv_obj_t *card_content = lv_obj_create(parent);
    lv_obj_set_size(card_content, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card_content, 12, 0);
    // lv_obj_set_style_pad_all(card_content, 10, 0);
    lv_obj_set_style_shadow_width(card_content, 10, 0);

    // add title
    lv_obj_t *title_label = lv_label_create(card_content);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    return card_content;
};