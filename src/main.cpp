// ESP32-S3 + ILI9341 dashboard: Pi 5 monitor (MQTT) + weather (Open-Meteo).
//
// Threading model
//   core 1  "lvgl"    : the only task that calls LVGL. Drains the app event
//                       queue, runs lv_timer_handler(), polls the nav button.
//   core 0  "wifi"    : connection supervisor, NTP setup
//           "mqtt"    : PubSubClient loop, parses pi5/stats
//           "weather" : HTTPS poll every 10 min
//   Network tasks only post copies of their data to the queue (app_events).

#include <Arduino.h>
#include <lvgl.h>

#include "app_config.h"
#include "app_events.h"
#include "display.h"
#include "mqtt.h"
#include "ui.h"
#include "weather.h"
#include "wifi_manager.h"

static void nav_button_poll() {
#if NAV_BUTTON_PIN >= 0
    static bool last = true;          // pulled up = released
    static uint32_t last_change = 0;
    bool now = digitalRead(NAV_BUTTON_PIN);
    if (now != last && millis() - last_change > 30) {
        last_change = millis();
        last = now;
        if (!now) ui_next_tile();     // falling edge = press
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
    ui_init();   // built before the LVGL task starts, so no race

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, nullptr, 4, nullptr, 1);

    wifi_start();
    mqtt_start();
    weather_start();

    Serial.printf("[main] heap free=%u\n", (unsigned)ESP.getFreeHeap());
}

void loop() { vTaskDelete(nullptr); }
