#pragma once
// Single source of truth for the connection state shown in the status bar.
// Written by the network tasks (wifi, mqtt, esp_event callbacks), read by the
// LVGL task from a 500 ms lv_timer. Never call LVGL from the writers.
#include <stdint.h>

struct NetState {
    bool     wifi_connected;   // STA associated *and* has an IP
    bool     mqtt_connected;
    int8_t   rssi;             // dBm, 0 when not connected
    uint32_t ip;               // IPv4, network byte order (0 = none)
};

NetState net_state_get();   // any task, returns a consistent snapshot
void net_state_set_wifi(bool connected, int8_t rssi, uint32_t ip);
void net_state_set_mqtt(bool connected);
