#pragma once
// Single MQTT client (PubSubClient, used only by the mqtt task). It owns the
// connection and routes messages; what the topics mean lives in the data
// services, which register handlers with mqtt_service_route() in their init:
//
//   pi5_service   pi5/stats, pi5/services
//   home_service  home/config, home/+/+/+/state      (publishes .../set)
//
// A new topic = one route in its service; this module does not change.
#include <stdint.h>

// Runs on the mqtt task. `payload` is not NUL-terminated.
typedef void (*MqttHandler)(const char *topic, const uint8_t *payload, unsigned len);

void mqtt_service_init();
void mqtt_service_task(void *arg);   // created by app.cpp

// Register before the task connects; every filter is subscribed on each
// (re)connect and messages go to the first route whose filter (+/#) matches.
bool mqtt_service_route(const char *filter, MqttHandler handler);

// Queue a publish (any task, never blocks). False if MQTT is down or the queue is full.
bool mqtt_service_publish(const char *topic, const char *payload);

bool mqtt_service_connected();   // any task
