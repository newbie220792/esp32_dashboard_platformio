#pragma once
#include <stdint.h>

// Single MQTT client for all topics. Incoming messages are routed by topic to
// a parser and posted to the UI as AppEvents; outgoing commands from the LVGL
// task go through a queue, so PubSubClient is only touched by the mqtt task.
//
// Subscribed:  pi5/stats, pi5/services, home/config, home/+/+/+/state
// Published:   home/<room>/<panel>/<id>/set  ("ON" / "OFF")
void mqtt_start();

bool mqtt_is_connected();   // any task

// Queue a switch command. Returns false if MQTT is down or the queue is full.
bool mqtt_request_switch(const char *room, const char *panel, uint16_t id, bool on);
