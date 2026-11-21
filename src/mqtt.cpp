#include "mqtt.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "app_config.h"
#include "app_events.h"
#include "secrets.h"
#include "wifi_manager.h"

static WiFiClient s_net;
static PubSubClient s_client(s_net);

static void on_message(char *topic, uint8_t *payload, unsigned int len) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) {
        Serial.printf("[mqtt] bad JSON on %s\n", topic);
        return;
    }
    AppEvent ev{};
    ev.type = AppEventType::PiStats;
    ev.pi.cpu = doc["cpu"] | 0.0f;
    ev.pi.ram = doc["ram"] | 0.0f;
    ev.pi.ram_used = doc["ram_used"] | 0.0f;
    ev.pi.ram_total = doc["ram_total"] | 0.0f;
    ev.pi.temp = doc["temp"] | 0.0f;
    app_events_post(ev);
}

static bool try_connect() {
    char id[32];
    snprintf(id, sizeof id, "esp32-dash-%06llx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF));
    const char *user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
    const char *pass = strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : nullptr;
    if (!s_client.connect(id, user, pass)) {
        Serial.printf("[mqtt] connect failed, rc=%d\n", s_client.state());
        return false;
    }
    s_client.subscribe(MQTT_TOPIC_STATS);
    Serial.printf("[mqtt] connected to %s:%d\n", MQTT_HOST, MQTT_PORT);
    return true;
}

static void mqtt_task(void *) {
    s_client.setServer(MQTT_HOST, MQTT_PORT);
    s_client.setCallback(on_message);
    s_client.setBufferSize(512);
    s_client.setKeepAlive(15);
    s_client.setSocketTimeout(5);

    bool was_connected = false;
    uint32_t backoff_ms = 1000;

    for (;;) {
        if (!wifi_is_connected()) {
            if (was_connected) {
                was_connected = false;
                app_post_flag(AppEventType::MqttState, false);
            }
            wifi_wait_connected(portMAX_DELAY);
        }

        if (!s_client.connected()) {
            if (was_connected) {
                was_connected = false;
                app_post_flag(AppEventType::MqttState, false);
            }
            if (try_connect()) {
                was_connected = true;
                backoff_ms = 1000;
                app_post_flag(AppEventType::MqttState, true);
            } else {
                vTaskDelay(pdMS_TO_TICKS(backoff_ms));
                backoff_ms = min<uint32_t>(backoff_ms * 2, 30000);
                continue;
            }
        }

        s_client.loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void mqtt_start() {
    xTaskCreatePinnedToCore(mqtt_task, "mqtt", 6144, nullptr, 2, nullptr, 0);
}
