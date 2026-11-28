#include "services/settings_service.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "app_config.h"
#include "core/config.h"
#include "core/state.h"
#include "hal/backlight.h"

// Callers are the LVGL task (screens, screen-off) and app_init; the mutex keeps
// read-modify-write of the settings and the backlight level consistent anyway.
static SemaphoreHandle_t s_lock;
static ScreenPower s_power = ScreenPower::On;

namespace {
struct Lock {
    Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_lock); }
};
}   // namespace

// Call with the lock held.
static void apply_backlight(const Settings &s) {
    uint8_t pct = s.brightness;
    if (s_power == ScreenPower::Off) pct = 0;
    else if (s_power == ScreenPower::Dim) pct = max<uint8_t>(5, (uint8_t)(s.brightness * SCREEN_DIM_PCT / 100));
    hal_backlight_set(pct);
}

void settings_service_init() {
    s_lock = xSemaphoreCreateMutex();
    const Settings s = config_load_settings();
    state_set_settings(s);
    Lock l;
    apply_backlight(s);
    Serial.printf("[settings] brightness=%u%% sleep=%us unit=%s\n", s.brightness, s.sleep_s,
                  s.unit == TempUnit::F ? "F" : "C");
}

void settings_service_set_brightness(uint8_t pct, bool persist) {
    if (pct < BRIGHTNESS_MIN) pct = BRIGHTNESS_MIN;
    if (pct > 100) pct = 100;
    Lock l;
    Settings s = state_get_settings();
    s.brightness = pct;
    state_set_settings(s);
    apply_backlight(s);
    if (persist) config_save_settings(s);
}

void settings_service_set_sleep(uint16_t seconds) {
    if (seconds != 0 && seconds != 15 && seconds != 30 && seconds != 60) return;
    Lock l;
    Settings s = state_get_settings();
    s.sleep_s = seconds;
    state_set_settings(s);
    config_save_settings(s);
}

void settings_service_set_unit(TempUnit unit) {
    Lock l;
    Settings s = state_get_settings();
    s.unit = unit;
    state_set_settings(s);
    config_save_settings(s);
}

void settings_service_set_power(ScreenPower p) {
    Lock l;
    if (p == s_power) return;
    s_power = p;
    apply_backlight(state_get_settings());
}
