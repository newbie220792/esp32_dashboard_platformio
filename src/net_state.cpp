#include "net_state.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static NetState s_state{};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

NetState net_state_get() {
    portENTER_CRITICAL(&s_mux);
    NetState copy = s_state;
    portEXIT_CRITICAL(&s_mux);
    return copy;
}

void net_state_set_wifi(bool connected, int8_t rssi, uint32_t ip) {
    portENTER_CRITICAL(&s_mux);
    s_state.wifi_connected = connected;
    s_state.rssi = connected ? rssi : 0;
    s_state.ip = connected ? ip : 0;
    portEXIT_CRITICAL(&s_mux);
}

void net_state_set_mqtt(bool connected) {
    portENTER_CRITICAL(&s_mux);
    s_state.mqtt_connected = connected;
    portEXIT_CRITICAL(&s_mux);
}
