#include "services/location_service.h"

#include <Arduino.h>
#include <string.h>

#include "app_config.h"
#include "core/config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/pi5_api.h"
#include "services/wifi_service.h"

#define TICK_MS 1000

void location_service_init() {
    Location loc{};
    if (!config_load_location(loc)) {
        strncpy(loc.city, DEFAULT_CITY, sizeof loc.city - 1);
        loc.lat = DEFAULT_LAT;
        loc.lon = DEFAULT_LON;
    }
    state_set_location(loc, false);
    Serial.printf("[location] start with %s (%.4f, %.4f)\n", loc.city, loc.lat, loc.lon);
}

// True = got an answer (changed or not).
static bool fetch(Pi5Api &api) {
    String body;
    const int code = pi5_api_get(api, "/api/location", body);
    if (code != 200) return false;
    Location loc{};
    if (!parse_location(body.c_str(), body.length(), loc)) {
        Serial.println("[location] bad /api/location response");
        return false;
    }
    const LocationState cur = state_get_location();
    const bool changed = strcmp(cur.loc.city, loc.city) || cur.loc.lat != loc.lat || cur.loc.lon != loc.lon;
    state_set_location(loc, true);   // also flips from_api on the first answer
    if (changed) {
        config_save_location(loc);
        Serial.printf("[location] %s (%.4f, %.4f)\n", loc.city, loc.lat, loc.lon);
    }
    return true;
}

void location_service_task(void *) {
    if (!pi5_api_configured()) {
        Serial.println("[location] PI5_API_HOST not set, keeping the saved/default location");
        vTaskDelete(nullptr);
    }
    static Pi5Api api;   // location task only
    bool was_up = false;
    uint32_t next_ms = 0;
    for (;;) {
        const bool up = wifi_service_is_connected();
        if (up && !was_up) next_ms = millis();   // boot / network back: ask now
        was_up = up;
        if (up && (int32_t)(millis() - next_ms) >= 0)
            next_ms = millis() + (fetch(api) ? LOCATION_REFRESH_MS : LOCATION_RETRY_MS);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
