#include "hal/backlight.h"

#include <Arduino.h>

#include "board_pins.h"

void hal_backlight_init() {
#if TFT_PIN_BL >= 0
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);   // fixed full brightness
#endif
}
