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

    static void updateWifiStatus(bool isConnected);

    static void updateCurrentTime(const char *currentTime);

    lv_obj_t *getPanel();

private:
    static lv_obj_t *time_label;
    static lv_obj_t *wifi_label;
    lv_obj_t *header_panel;
};
#endif
