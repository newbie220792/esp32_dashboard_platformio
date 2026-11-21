#include "mqtt.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <string.h>

#include "app_config.h"
#include "app_events.h"
#include "parsers.h"
#include "secrets.h"
#include "wifi_manager.h"

#define MQTT_BUFFER_SIZE 4096   // home/config can be a few KB
#define CMD_QUEUE_LEN    8

struct MqttCmd {
    char topic[64];
    char payload[4];
};

static WiFiClient s_net;
static PubSubClient s_client(s_net);
static QueueHandle_t s_cmd_queue;
static volatile bool s_connected = false;

// ---------------- topic handlers (run on the mqtt task) ----------------

static void on_stats(const char *, const uint8_t *payload, unsigned len) {
    AppEvent ev{};
    ev.type = AppEventType::PiStats;
    if (parse_pi_stats((const char *)payload, len, ev.pi)) app_events_post(ev);
    else Serial.println("[mqtt] bad pi5/stats JSON");
}

static void on_services(const char *, const uint8_t *payload, unsigned len) {
    ServiceList *list = new ServiceList();
    if (!parse_services((const char *)payload, len, *list)) {
        Serial.println("[mqtt] bad pi5/services JSON, keeping old list");
        delete list;
        return;
    }
    AppEvent ev{};
    ev.type = AppEventType::Services;
    ev.services = list;
    if (!app_events_post(ev)) delete list;
}

static void on_home_config(const char *, const uint8_t *payload, unsigned len) {
    HomeConfig *cfg = new HomeConfig();
    if (!parse_home_config((const char *)payload, len, *cfg)) {
        Serial.printf("[mqtt] bad home/config JSON (%u bytes), keeping old config\n", len);
        delete cfg;
        return;
    }
    AppEvent ev{};
    ev.type = AppEventType::HomeConfig;
    ev.home = cfg;
    if (!app_events_post(ev)) delete cfg;
}

static void on_switch_state(const char *topic, const uint8_t *payload, unsigned len) {
    AppEvent ev{};
    ev.type = AppEventType::SwitchState;
    if (parse_switch_state(topic, (const char *)payload, len, ev.sw)) app_events_post(ev);
}

// Topic router: every filter is subscribed on connect, and each incoming
// message goes to the first route whose filter matches.
struct Route {
    const char *filter;
    void (*handler)(const char *topic, const uint8_t *payload, unsigned len);
};

static const Route kRoutes[] = {
    {MQTT_TOPIC_STATS, on_stats},
    {MQTT_TOPIC_SERVICES, on_services},
    {MQTT_TOPIC_HOME_CONFIG, on_home_config},
    {MQTT_TOPIC_HOME_STATE, on_switch_state},
};

static void on_message(char *topic, uint8_t *payload, unsigned int len) {
    for (const Route &r : kRoutes) {
        if (mqtt_topic_matches(r.filter, topic)) {
            r.handler(topic, payload, len);
            return;
        }
    }
}

// ---------------- connection ----------------

static void set_connected(bool c) {
    if (s_connected == c) return;
    s_connected = c;
    app_post_flag(AppEventType::MqttState, c);
    if (!c) xQueueReset(s_cmd_queue);   // don't replay stale taps after reconnect
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
    for (const Route &r : kRoutes) s_client.subscribe(r.filter);
    Serial.printf("[mqtt] connected to %s:%d\n", MQTT_HOST, MQTT_PORT);
    return true;
}

static void drain_commands() {
    MqttCmd cmd;
    while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
        if (!s_client.publish(cmd.topic, cmd.payload)) Serial.printf("[mqtt] publish failed: %s\n", cmd.topic);
    }
}

static void mqtt_task(void *) {
    s_client.setServer(MQTT_HOST, MQTT_PORT);
    s_client.setCallback(on_message);
    if (!s_client.setBufferSize(MQTT_BUFFER_SIZE)) Serial.println("[mqtt] buffer alloc failed");
    s_client.setKeepAlive(15);
    s_client.setSocketTimeout(5);

    uint32_t backoff_ms = 1000;
    for (;;) {
        if (!wifi_is_connected()) {
            set_connected(false);
            wifi_wait_connected(portMAX_DELAY);
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

void mqtt_start() {
    s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(MqttCmd));
    // Parsing home/config with ArduinoJson needs some stack headroom.
    xTaskCreatePinnedToCore(mqtt_task, "mqtt", 8192, nullptr, 2, nullptr, 0);
}

bool mqtt_is_connected() { return s_connected; }

bool mqtt_request_switch(const char *room, const char *panel, uint16_t id, bool on) {
    if (!s_connected || !s_cmd_queue) return false;
    MqttCmd cmd;
    if (!build_switch_set_topic(cmd.topic, sizeof cmd.topic, room, panel, id)) return false;
    strcpy(cmd.payload, on ? "ON" : "OFF");
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE;
}
