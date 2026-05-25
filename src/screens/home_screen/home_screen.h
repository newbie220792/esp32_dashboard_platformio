#pragma once

#ifndef LV_HOME_SCREEN_H
#define LV_HOME_SCREEN_H

#include <lvgl.h>
#include <Arduino.h>

class HomeScreen
{
public:
HomeScreen(lv_obj_t*parent);    

private: 
lv_obj_t *panel;
String *title;
};

#endif