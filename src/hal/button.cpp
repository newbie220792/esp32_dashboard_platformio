#include "hal/button.h"

#include <Arduino.h>

#include "board_pins.h"

void hal_button_init() {
#if NAV_BUTTON_PIN >= 0
    pinMode(NAV_BUTTON_PIN, INPUT_PULLUP);
#endif
}

ButtonEvent hal_button_poll(uint32_t long_ms) {
#if NAV_BUTTON_PIN >= 0
    static bool pressed = false;
    static bool long_fired = false;
    static uint32_t down_ms = 0, last_change = 0;
    const bool down = digitalRead(NAV_BUTTON_PIN) == LOW;
    const uint32_t now = millis();

    if (down != pressed && now - last_change > 30) {   // debounce
        last_change = now;
        pressed = down;
        if (down) {
            down_ms = now;
            long_fired = false;
        } else if (!long_fired) {
            return ButtonEvent::Short;                  // released before the hold
        }
    }
    if (pressed && !long_fired && now - down_ms >= long_ms) {
        long_fired = true;
        return ButtonEvent::Long;
    }
#endif
    return ButtonEvent::None;
}
