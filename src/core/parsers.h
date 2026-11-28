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

// ---------------- Pi 5 REST API (task 8) ----------------

// Copies UTF-8 `src` into `dst` keeping only what the built-in Montserrat
// fonts can draw: printable ASCII, the degree sign, and the bullet (a middle
// dot becomes a bullet). Any other character becomes '?', control characters
// a space. Never splits a multi-byte character; always NUL-terminates.
void ascii_sanitize(char *dst, size_t dst_len, const char *src);

// "critical" / "warning" / "info"; anything else (or null) is Info.
AlertLevel parse_alert_level(const char *s);

// GET /api/alerts/summary: {"unacked":3,"critical":1,"latest_id":128}
bool parse_alerts_summary(const char *json, size_t len, AlertSummary &out);

// GET /api/alerts: {"items":[{id, level, title, detail, ts, count, ack}]}.
// Keeps at most ALERTS_MAX items (the Pi 5 should never send more), skips
// items without an id or title, sanitizes text. False (and `out` untouched)
// on malformed JSON or a missing "items" array.
bool parse_alerts(const char *json, size_t len, AlertList &out);

// GET /api/location: {"city":"TP Dong Nai","lat":10.95,"lon":106.84}.
// False if the city is empty or lat/lon are missing or out of range.
bool parse_location(const char *json, size_t len, Location &out);

// "now" (< 1 min), "N min", "N h", "N d" between epoch `ts` and `now`.
// Empty string when the clock isn't set or `ts` is in the future by > 1 min.
void format_age(char *buf, size_t buf_len, uint32_t ts, uint32_t now);

// ---------------- System info screen (task 9) ----------------

// Lit bars (0..4) for a Wi-Fi RSSI: >= -55 dBm 4, -56..-65 3, -66..-75 2,
// -76..-85 1, below -85 0. 0 also for rssi >= 0 (no reading).
int rssi_bars(int rssi_dbm);

// "2d 4h 12m" (>= 1 day), "4h 12m" (>= 1 hour), "12m".
void format_uptime(char *buf, size_t buf_len, uint64_t seconds);

// One size: "182 KB" below 1 MB (whole KB), else "1.48 MB" (2 decimals,
// trailing zeros dropped: "8 MB", "7.1 MB"). 1 KB = 1024 B.
void format_size(char *buf, size_t buf_len, uint64_t bytes);

// "182 / 320 KB", "7.1 / 8 MB", "1.62 / 1.94 MB": both numbers in the unit
// that fits `total` (KB below 1 MB, else MB).
void format_size_pair(char *buf, size_t buf_len, uint64_t part, uint64_t total);
