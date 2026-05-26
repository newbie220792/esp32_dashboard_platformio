/**
 * @file header.h
 *
 */

#pragma once

#ifndef LV_HEADER_H
#define LV_HEADER_H

#include <lvgl.h>
#include <stdio.h>
#include <Arduino.h>
class HeaderPanel
{
public:
    void init(lv_obj_t *parent);

    void updateWifiStatus(bool isConnected);

    void updateCurrentTime(const char *currentTime);

    lv_obj_t *getPanel();

private:
    lv_obj_t *time_label;
    lv_obj_t *wifi_label;
    lv_obj_t *header_panel;
};
#endif
