#pragma once
// MQTT payload / topic parsing. Pure functions (ArduinoJson only), unit-tested
// on the host with `pio test -e native`.

#include "core/models.h"

bool parse_pi_stats(const char *json, size_t len, PiStats &out);

// Returns false on malformed JSON or a missing "services" array; `out` is
// left untouched in that case so the caller keeps the previous list.
bool parse_services(const char *json, size_t len, ServiceList &out);

// Same contract as parse_services. Rooms/panels without a usable id are
// skipped; panels are padded/truncated to exactly HOME_SWITCHES switches.
bool parse_home_config(const char *json, size_t len, HomeConfig &out);

// "home/<room>/<panel>/<id>/state" + "ON"/"OFF" -> SwitchState.
bool parse_switch_state(const char *topic, const char *payload, size_t len, SwitchState &out);

// Builds "home/<room>/<panel>/<id>/set". Returns false if it doesn't fit.
bool build_switch_set_topic(char *buf, size_t buf_len, const char *room, const char *panel, uint16_t id);

SwitchIcon parse_switch_icon(const char *name);

// MQTT filter match with '+' (one level) and '#' (rest), e.g. home/+/+/+/state.
bool mqtt_topic_matches(const char *filter, const char *topic);

// Topic-segment safety: non-empty and no '/', '+', '#'.
bool is_valid_topic_id(const char *id);

// Wi-Fi failure reason from the last STA disconnect reason code
// (wifi_err_reason_t, 0 = none seen) and Arduino wl_status_t value.
WifiFailReason wifi_fail_reason(uint8_t disconnect_reason, int wl_status);
const char *wifi_fail_reason_text(WifiFailReason r);

// Wi-Fi join QR payload: WIFI:T:WPA;S:<ssid>;P:<pass>;;  (T:nopass when pass
// is empty). Escapes \ ; , : " as the format requires. False if it doesn't fit.
bool build_wifi_qr(char *buf, size_t buf_len, const char *ssid, const char *pass);
