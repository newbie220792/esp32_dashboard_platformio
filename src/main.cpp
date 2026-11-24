// JC4827W543C (ESP32-S3, NV3041A 480x272) dashboard: Pi 5 monitor (MQTT) + weather (Open-Meteo).
//
// Threading model
//   core 1  "lvgl"    : the only task that calls LVGL. Drains the app event
//                       queue, runs lv_timer_handler() (which also polls the
//                       GT911 touch panel), polls the nav button.
//   core 0  "wifi"    : boot-time setup flow (secrets.h -> NVS -> WiFiManager
//                       portal), then reconnect supervision, NTP setup
//           "mqtt"    : PubSubClient loop, parses pi5/stats
//           "weather" : HTTPS poll every 10 min (backoff on errors)
//   Network tasks only post copies of their data to the queue (app_events).

#include <Arduino.h>
#include <lvgl.h>

#include "app_config.h"
#include "app_events.h"
#include "display.h"
#include "mqtt.h"
#include "touch.h"
#include "ui.h"
#include "weather.h"
#include "wifi_manager.h"
#include "wifi_setup.h"

// BOOT button: short press = next tile; hold WIFI_RESET_HOLD_MS = erase the
// saved Wi-Fi and open the setup portal.
static void nav_button_poll() {
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
            ui_next_tile();                             // released before the hold
        }
    }
    if (pressed && !long_fired && now - down_ms >= WIFI_RESET_HOLD_MS) {
        long_fired = true;
        Serial.println("[main] BOOT held: resetting Wi-Fi");
        wifi_setup_command(WifiCmd::ResetAndPortal);
        ui_wifi_reopen();                               // undo an earlier "Back"
    }
#endif
}

static void lvgl_task(void *) {
    for (;;) {
        ui_process_events();
        nav_button_poll();
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY || wait_ms > 20) wait_ms = 20;
        if (wait_ms < 2) wait_ms = 2;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

void setup() {
    Serial.begin(115200);
#if NAV_BUTTON_PIN >= 0
    pinMode(NAV_BUTTON_PIN, INPUT_PULLUP);
#endif

    app_events_init();
    display_init();
    touch_init();
    ui_init();   // built before the LVGL task starts, so no race

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 4, nullptr, 1);

    wifi_start();
    mqtt_start();
    weather_start();

    Serial.printf("[main] heap free=%u\n", (unsigned)ESP.getFreeHeap());
}

void loop() { vTaskDelete(nullptr); }
