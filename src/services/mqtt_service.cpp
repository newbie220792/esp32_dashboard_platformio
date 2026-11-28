#include "services/mqtt_service.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <string.h>

#include "core/config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/wifi_service.h"

#define MQTT_BUFFER_SIZE 4096   // home/config can be a few KB
#define CMD_QUEUE_LEN    8
#define MAX_ROUTES       8

struct MqttCmd {
    char topic[64];
    char payload[8];
};

struct Route {
    const char *filter;
    MqttHandler handler;
};

static WiFiClient s_net;
static PubSubClient s_client(s_net);
static QueueHandle_t s_cmd_queue;
static volatile bool s_connected = false;
static Route s_routes[MAX_ROUTES];
static int s_route_count = 0;

static void on_message(char *topic, uint8_t *payload, unsigned int len) {
    for (int i = 0; i < s_route_count; i++) {
        if (mqtt_topic_matches(s_routes[i].filter, topic)) {
            s_routes[i].handler(topic, payload, len);
            return;
        }
    }
}

static void set_connected(bool c) {
    if (s_connected == c) return;
    s_connected = c;
    state_set_mqtt(c);
    if (!c) xQueueReset(s_cmd_queue);   // don't replay stale taps after reconnect
}

static bool try_connect() {
    char id[32];
    snprintf(id, sizeof id, "esp32-dash-%06llx", (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFF));
    const char *user = strlen(config_mqtt_user()) ? config_mqtt_user() : nullptr;
    const char *pass = strlen(config_mqtt_password()) ? config_mqtt_password() : nullptr;
    if (!s_client.connect(id, user, pass)) {
        Serial.printf("[mqtt] connect failed, rc=%d\n", s_client.state());
        return false;
    }
    for (int i = 0; i < s_route_count; i++) s_client.subscribe(s_routes[i].filter);
    Serial.printf("[mqtt] connected to %s:%u\n", config_mqtt_host(), (unsigned)config_mqtt_port());
    return true;
}

static void drain_commands() {
    MqttCmd cmd;
    while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
        if (!s_client.publish(cmd.topic, cmd.payload)) Serial.printf("[mqtt] publish failed: %s\n", cmd.topic);
    }
}

void mqtt_service_init() { s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(MqttCmd)); }

void mqtt_service_task(void *) {
    s_client.setServer(config_mqtt_host(), config_mqtt_port());
    s_client.setCallback(on_message);
    if (!s_client.setBufferSize(MQTT_BUFFER_SIZE)) Serial.println("[mqtt] buffer alloc failed");
    s_client.setKeepAlive(15);
    s_client.setSocketTimeout(5);

    uint32_t backoff_ms = 1000;
    for (;;) {
        if (!wifi_service_is_connected()) {
            set_connected(false);
            wifi_service_wait_connected(portMAX_DELAY);
        }
        if (!s_client.connected()) {
            set_connected(false);
            if (!try_connect()) {
                vTaskDelay(pdMS_TO_TICKS(backoff_ms));
                backoff_ms = min<uint32_t>(backoff_ms * 2, 30000);
                continue;
            }
            backoff_ms = 1000;
            set_connected(true);
        }
        s_client.loop();
        drain_commands();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool mqtt_service_route(const char *filter, MqttHandler handler) {
    if (s_route_count >= MAX_ROUTES) return false;
    s_routes[s_route_count++] = {filter, handler};
    return true;
}

bool mqtt_service_publish(const char *topic, const char *payload) {
    if (!s_connected || !s_cmd_queue) return false;
    MqttCmd cmd;
    if (strlen(topic) >= sizeof cmd.topic || strlen(payload) >= sizeof cmd.payload) return false;
    strcpy(cmd.topic, topic);
    strcpy(cmd.payload, payload);
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE;
}

bool mqtt_service_connected() { return s_connected; }
