#include "app/app.h"

#include <Arduino.h>

#include "app_config.h"
#include "core/events.h"
#include "core/state.h"
#include "hal/backlight.h"
#include "hal/button.h"
#include "hal/display.h"
#include "hal/touch.h"
#include "services/home_service.h"
#include "services/mqtt_service.h"
#include "services/pi5_service.h"
#include "services/time_service.h"
#include "services/weather_service.h"
#include "services/wifi_service.h"
#include "ui/ui_manager.h"

// Threading model
//   core 1  "lvgl"    : the only task that calls LVGL (ui_task)
//           "input"   : BOOT button -> events / Wi-Fi command
//   core 0  "wifi"    : setup flow (secrets.h -> NVS -> portal), then reconnects
//           "time"    : starts SNTP once Wi-Fi is up, then exits
//           "mqtt"    : PubSubClient loop + topic routing to the data services
//           "home"    : switch pending timeouts
//           "weather" : HTTPS poll every 10 min (TLS needs the deep stack)
//   Services write the state store; the UI polls it (core/state.h).
struct TaskDef {
    TaskFunction_t fn;
    const char *name;
    uint32_t stack;
    UBaseType_t prio;
    BaseType_t core;
};

static const TaskDef kTasks[] = {
    {ui_task, "lvgl", 8192, 4, 1},
    {wifi_service_task, "wifi", 8192, 3, 0},       // WiFiManager web server + connect attempts
    {time_service_task, "time", 3072, 1, 0},
    {mqtt_service_task, "mqtt", 8192, 2, 0},       // ArduinoJson on home/config
    {home_service_task, "home", 3072, 1, 0},
    {weather_service_task, "weather", 12288, 1, 0},
};

// BOOT: short press = next tile; hold WIFI_RESET_HOLD_MS = erase the saved
// Wi-Fi and open the setup portal.
static void input_task(void *) {
    for (;;) {
        switch (hal_button_poll(WIFI_RESET_HOLD_MS)) {
        case ButtonEvent::Short: events_post(EventType::NavNext); break;
        case ButtonEvent::Long:
            Serial.println("[app] BOOT held: resetting Wi-Fi");
            wifi_service_command(WifiCmd::ResetAndPortal);
            events_post(EventType::WifiReopen);   // undo an earlier "Back"
            break;
        case ButtonEvent::None: break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_init() {
    Serial.begin(115200);

    // core
    state_init();
    events_init();

    // hal + LVGL
    hal_button_init();
    hal_display_init();
    hal_backlight_init();
    hal_touch_init();

    // ui: built before the LVGL task starts, so no race
    ui_init();

    // services: data services register their MQTT routes before mqtt runs
    wifi_service_init();
    time_service_init();
    mqtt_service_init();
    pi5_service_init();
    home_service_init();
    weather_service_init();

    for (const TaskDef &t : kTasks) xTaskCreatePinnedToCore(t.fn, t.name, t.stack, nullptr, t.prio, nullptr, t.core);
    xTaskCreatePinnedToCore(input_task, "input", 2048, nullptr, 3, nullptr, 1);

    Serial.printf("[app] heap free=%u\n", (unsigned)ESP.getFreeHeap());
}
