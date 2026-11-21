#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "app_config.h"
#include "app_events.h"
#include "secrets.h"

#define BIT_CONNECTED BIT0

static EventGroupHandle_t s_eg;

// Runs on the default esp_event loop task: only touch FreeRTOS primitives.
static void on_got_ip(void *, esp_event_base_t, int32_t, void *) {
    xEventGroupSetBits(s_eg, BIT_CONNECTED);
    app_post_flag(AppEventType::WifiState, true);
}

static void on_disconnected(void *, esp_event_base_t, int32_t, void *) {
    xEventGroupClearBits(s_eg, BIT_CONNECTED);
    app_post_flag(AppEventType::WifiState, false);
}

static void wifi_task(void *) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t down_since = millis();
    bool time_configured = false;

    for (;;) {
        if (wifi_is_connected()) {
            down_since = millis();
            if (!time_configured) {
                configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
                time_configured = true;
                Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
            }
        } else if (millis() - down_since > 20000) {
            // Arduino's auto-reconnect occasionally gives up; kick it.
            Serial.println("[wifi] reconnecting...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            down_since = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void wifi_start() {
    s_eg = xEventGroupCreate();
    WiFi.mode(WIFI_STA);            // also creates netif + default event loop
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, nullptr, nullptr);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnected, nullptr, nullptr);
    xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, nullptr, 3, nullptr, 0);
}

bool wifi_is_connected() {
    return s_eg && (xEventGroupGetBits(s_eg) & BIT_CONNECTED);
}

bool wifi_wait_connected(TickType_t timeout) {
    return xEventGroupWaitBits(s_eg, BIT_CONNECTED, pdFALSE, pdTRUE, timeout) & BIT_CONNECTED;
}
