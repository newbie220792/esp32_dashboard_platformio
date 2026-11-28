#pragma once
// BOOT button (NAV_BUTTON_PIN, active low) with debounce and long-press detection.
#include <stdint.h>

enum class ButtonEvent : uint8_t { None, Short, Long };

void hal_button_init();
// Call every ~10 ms from one task. Short = released before `long_ms`;
// Long = fires once while still held for `long_ms`.
ButtonEvent hal_button_poll(uint32_t long_ms);
