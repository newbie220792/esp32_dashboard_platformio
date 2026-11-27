#include "hal/backlight.h"

#include <Arduino.h>

#include "board_pins.h"

#define BL_LEDC_CH   7       // high channel: the display driver doesn't use LEDC
#define BL_FREQ_HZ   20000   // above hearing range (no coil whine)
#define BL_BITS      8

void hal_backlight_init() {
#if TFT_PIN_BL >= 0
    ledcSetup(BL_LEDC_CH, BL_FREQ_HZ, BL_BITS);
    ledcAttachPin(TFT_PIN_BL, BL_LEDC_CH);
    ledcWrite(BL_LEDC_CH, (1 << BL_BITS) - 1);
#endif
}

void hal_backlight_set(uint8_t pct) {
#if TFT_PIN_BL >= 0
    if (pct > 100) pct = 100;
    ledcWrite(BL_LEDC_CH, (uint32_t)pct * ((1 << BL_BITS) - 1) / 100);
#else
    (void)pct;
#endif
}
