#pragma once
// Backlight on TFT_PIN_BL, dimmed with LEDC PWM.
#include <stdint.h>

void hal_backlight_init();              // full brightness until hal_backlight_set()
void hal_backlight_set(uint8_t pct);    // 0 = off, 100 = full; any task
