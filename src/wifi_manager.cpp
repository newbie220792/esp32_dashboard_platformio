#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "app_config.h"
#include "app_events.h"
#include "wifi_setup.h"

#define BIT_CONNECTED BIT0

static EventGroupHandle_t s_eg;

// Runs on the default esp_event loop task: only touch FreeRTOS primitives.
static void on_got_ip(void *, esp_event_base_t, int32_t, void *) {
    xEventGroupSetBits(s_eg, BIT_CONNECTED);
    app_post_flag(AppEventType::WifiState, true);
}

static void on_disconnected(void *, esp_event_base_t, int32_t, void *data) {
    xEventGroupClearBits(s_eg, BIT_CONNECTED);
    app_post_flag(AppEventType::WifiState, false);
    auto *info = static_cast<wifi_event_sta_disconnected_t *>(data);
    wifi_setup_note_disconnect(info ? info->reason : 0);
}

static void wifi_task(void *) {
    char ssid[33], pass[65];
    wifi_setup_run(false, ssid, sizeof ssid, pass, sizeof pass);
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());

    uint32_t down_since = millis();
    for (;;) {
        WifiCmd cmd;
        if (wifi_setup_take_command(cmd) && cmd == WifiCmd::ResetAndPortal) {
            // BOOT held: forget the saved network and reconfigure.
            wifi_setup_erase_saved();
            WiFi.disconnect();
            wifi_setup_run(true, ssid, sizeof ssid, pass, sizeof pass);
        }

        if (wifi_is_connected()) {
            down_since = millis();
        } else if (millis() - down_since > 20000) {
            // Arduino's auto-reconnect occasionally gives up; kick it.
            Serial.println("[wifi] reconnecting...");
            WiFi.disconnect();
            WiFi.begin(ssid, pass);
            down_since = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void wifi_start() {
    s_eg = xEventGroupCreate();
    wifi_setup_init();
    WiFi.persistent(false);         // credentials live in our own NVS namespace
    WiFi.mode(WIFI_STA);            // also creates netif + default event loop
    WiFi.setSleep(false);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, nullptr, nullptr);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnected, nullptr, nullptr);
    // WiFiManager's web server + connect attempts need more stack than before.
    xTaskCreatePinnedToCore(wifi_task, "wifi", 8192, nullptr, 3, nullptr, 0);
}

bool wifi_is_connected() {
    return s_eg && (xEventGroupGetBits(s_eg) & BIT_CONNECTED);
}

bool wifi_wait_connected(TickType_t timeout) {
    return xEventGroupWaitBits(s_eg, BIT_CONNECTED, pdFALSE, pdTRUE, timeout) & BIT_CONNECTED;
}
