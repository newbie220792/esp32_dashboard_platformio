#pragma once
// Settings screen values (task 8): brightness, screen-off delay, temperature
// unit. Loaded from NVS and applied before the UI is drawn; each change is
// written to StateGroup::Settings at once. No task: every call is short.
#include <stdint.h>

#include "core/models.h"

// Reads NVS, sets the backlight and the state store. Call before ui_init().
void settings_service_init();

// Slider drag: `persist` false applies only; on release call again with true.
void settings_service_set_brightness(uint8_t pct, bool persist);
void settings_service_set_sleep(uint16_t seconds);   // 15 / 30 / 60, 0 = never
void settings_service_set_unit(TempUnit unit);

// Screen-off driver (ui_manager, from LVGL inactivity): the backlight level
// relative to the user's brightness. Not saved.
enum class ScreenPower : uint8_t { On, Dim, Off };
void settings_service_set_power(ScreenPower p);
