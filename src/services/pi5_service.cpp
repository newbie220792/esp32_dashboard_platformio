#include "services/pi5_service.h"

#include <Arduino.h>

#include "app_config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/mqtt_service.h"

static void on_stats(const char *, const uint8_t *payload, unsigned len) {
    PiStats s{};
    if (parse_pi_stats((const char *)payload, len, s)) state_set_pi5_stats(s);
    else Serial.println("[pi5] bad pi5/stats JSON");
}

static void on_services(const char *, const uint8_t *payload, unsigned len) {
    static ServiceList list;   // mqtt task only; keeps it off the task stack
    if (!parse_services((const char *)payload, len, list)) {
        Serial.println("[pi5] bad pi5/services JSON, keeping old list");
        return;
    }
    state_set_services(list);
}

void pi5_service_init() {
    mqtt_service_route(MQTT_TOPIC_STATS, on_stats);
    mqtt_service_route(MQTT_TOPIC_SERVICES, on_services);
}
