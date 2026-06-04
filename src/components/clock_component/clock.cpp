#include "clock.h"

#include <time.h>
#include <Arduino.h>

lv_obj_t *ClockWidget::time_label = nullptr;
lv_timer_t *ClockWidget::timer = nullptr;

void ClockWidget::update_time(lv_timer_t *timer)
{
    if (time_label == nullptr)
        return;

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        lv_label_set_text(time_label, "--:--:--");
        return;
    }

    char buf[16];

    strftime(
        buf,
        sizeof(buf),
        "%H:%M:%S",
        &timeinfo);

    lv_label_set_text(time_label, buf);
}

void ClockWidget::init(lv_obj_t *parent)
{
    time_label = lv_label_create(parent);

    lv_obj_set_style_text_font(
        time_label,
        &lv_font_montserrat_14,
        0);

    lv_label_set_text(
        time_label,
        "--:--:--");

    // lv_obj_center(time_label);
    lv_obj_set_align(time_label, LV_ALIGN_LEFT_MID);

    update_time(nullptr);

    timer = lv_timer_create(
        update_time,
        1000,
        nullptr);
}