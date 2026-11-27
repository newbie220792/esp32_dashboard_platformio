#include "core/parsers.h"

#include <ArduinoJson.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>

static void copy_str(char *dst, size_t n, const char *src) {
    if (!src) src = "";
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

bool is_valid_topic_id(const char *id) {
    if (!id || !*id) return false;
    for (const char *p = id; *p; p++)
        if (*p == '/' || *p == '+' || *p == '#') return false;
    return true;
}

bool mqtt_topic_matches(const char *f, const char *t) {
    while (*f) {
        if (*f == '#') return true;                 // matches the remainder
        if (*f == '+') {
            if (*t == '/' && f[1] == '\0') return false;
            while (*t && *t != '/') t++;            // consume one level
            f++;
        } else {
            if (*f != *t) return false;
            f++;
            t++;
        }
    }
    return *t == '\0';
}

bool parse_pi_stats(const char *json, size_t len, PiStats &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len) || !doc.is<JsonObject>()) return false;
    out.cpu = doc["cpu"] | 0.0f;
    out.ram = doc["ram"] | 0.0f;
    out.ram_used = doc["ram_used"] | 0.0f;
    out.ram_total = doc["ram_total"] | 0.0f;
    out.temp = doc["temp"] | 0.0f;
    return true;
}

bool parse_services(const char *json, size_t len, ServiceList &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;
    JsonArray arr = doc["services"];
    if (arr.isNull()) return false;

    ServiceList tmp{};
    for (JsonObject s : arr) {
        if (tmp.count >= MAX_SERVICES) break;
        const char *name = s["name"];
        if (!name || !*name) continue;
        ServiceInfo &it = tmp.items[tmp.count++];
        copy_str(it.name, sizeof it.name, name);
        const char *status = s["status"];
        it.running = status && strcmp(status, "running") == 0;
        JsonVariant port = s["port"];
        it.port = port.is<int>() ? port.as<int>() : -1;
    }
    out = tmp;
    return true;
}

SwitchIcon parse_switch_icon(const char *name) {
    if (!name) return SwitchIcon::Bulb;
    if (!strcmp(name, "fan")) return SwitchIcon::Fan;
    if (!strcmp(name, "plug")) return SwitchIcon::Plug;
    if (!strcmp(name, "snow") || !strcmp(name, "ac")) return SwitchIcon::Snow;
    return SwitchIcon::Bulb;
}

static bool parse_on_off(const char *s, size_t len, bool &on) {
    if (len == 2 && !strncmp(s, "ON", 2)) { on = true; return true; }
    if (len == 3 && !strncmp(s, "OFF", 3)) { on = false; return true; }
    return false;
}

bool parse_home_config(const char *json, size_t len, HomeConfig &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;
    JsonArray rooms = doc["rooms"];
    if (rooms.isNull()) return false;

    // HomeConfig is ~5 KB: build in place only after the JSON parsed, so a
    // bad payload never clobbers the caller's copy.
    out.room_count = 0;
    for (JsonObject r : rooms) {
        if (out.room_count >= HOME_MAX_ROOMS) break;
        const char *rid = r["id"];
        if (!is_valid_topic_id(rid)) continue;
        HomeRoom &room = out.rooms[out.room_count++];
        room = HomeRoom{};
        copy_str(room.id, sizeof room.id, rid);
        copy_str(room.name, sizeof room.name, r["name"] | rid);

        for (JsonObject p : r["panels"].as<JsonArray>()) {
            if (room.panel_count >= HOME_MAX_PANELS) break;
            const char *pid = p["id"];
            if (!is_valid_topic_id(pid)) continue;
            HomePanel &panel = room.panels[room.panel_count++];
            copy_str(panel.id, sizeof panel.id, pid);
            copy_str(panel.name, sizeof panel.name, p["name"] | pid);

            uint8_t n = 0;
            for (JsonObject s : p["switches"].as<JsonArray>()) {
                if (n >= HOME_SWITCHES) break;
                HomeSwitch &sw = panel.switches[n];
                sw.present = true;
                sw.id = s["id"] | (n + 1);
                copy_str(sw.name, sizeof sw.name, s["name"] | "");
                sw.icon = parse_switch_icon(s["icon"]);
                const char *st = s["state"] | "OFF";
                bool on = false;
                parse_on_off(st, strlen(st), on);
                sw.on = on;
                n++;
            }
        }
    }
    return true;
}

bool parse_switch_state(const char *topic, const char *payload, size_t len, SwitchState &out) {
    // Expect exactly: home / <room> / <panel> / <id> / state
    char buf[96];
    size_t tl = strlen(topic);
    if (tl >= sizeof buf) return false;
    memcpy(buf, topic, tl + 1);

    char *parts[5];
    int n = 0;
    char *p = buf;
    for (;;) {
        if (n == 5) return false;          // too many segments
        parts[n++] = p;
        char *slash = strchr(p, '/');
        if (!slash) break;
        *slash = '\0';
        p = slash + 1;
    }
    if (n != 5) return false;
    for (int i = 0; i < 5; i++)
        if (!*parts[i]) return false;      // empty segment
    if (strcmp(parts[0], "home") || strcmp(parts[4], "state")) return false;
    if (strlen(parts[1]) >= HOME_ID_LEN || strlen(parts[2]) >= HOME_ID_LEN) return false;

    char *end;
    long id = strtol(parts[3], &end, 10);
    if (*end || id < 0 || id > 65535) return false;

    bool on;
    if (!parse_on_off(payload, len, on)) return false;

    copy_str(out.room, sizeof out.room, parts[1]);
    copy_str(out.panel, sizeof out.panel, parts[2]);
    out.id = (uint16_t)id;
    out.on = on;
    return true;
}

bool build_switch_set_topic(char *buf, size_t buf_len, const char *room, const char *panel, uint16_t id) {
    int n = snprintf(buf, buf_len, "home/%s/%s/%u/set", room, panel, (unsigned)id);
    return n > 0 && (size_t)n < buf_len;
}

WifiFailReason wifi_fail_reason(uint8_t reason, int wl_status) {
    switch (reason) {
    case 201:            // WIFI_REASON_NO_AP_FOUND
        return WifiFailReason::NotFound;
    case 2:              // WIFI_REASON_AUTH_EXPIRE
    case 15:             // WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT
    case 202:            // WIFI_REASON_AUTH_FAIL
    case 204:            // WIFI_REASON_HANDSHAKE_TIMEOUT
        return WifiFailReason::WrongPassword;
    case 0:
        break;
    default:
        return WifiFailReason::ConnectFailed;
    }
    if (wl_status == 1) return WifiFailReason::NotFound;        // WL_NO_SSID_AVAIL
    if (wl_status == 4) return WifiFailReason::ConnectFailed;   // WL_CONNECT_FAILED
    return WifiFailReason::Timeout;
}

const char *wifi_fail_reason_text(WifiFailReason r) {
    switch (r) {
    case WifiFailReason::NotFound:      return "Network not found";
    case WifiFailReason::WrongPassword: return "Wrong password";
    case WifiFailReason::ConnectFailed: return "Connection failed";
    case WifiFailReason::Timeout:       return "Timeout";
    default:                            return "";
    }
}

static bool qr_append(char *buf, size_t buf_len, size_t &pos, const char *s, bool escape) {
    for (; *s; s++) {
        if (escape && strchr("\\;,:\"", *s)) {
            if (pos + 1 >= buf_len) return false;
            buf[pos++] = '\\';
        }
        if (pos + 1 >= buf_len) return false;
        buf[pos++] = *s;
    }
    buf[pos] = '\0';
    return true;
}

// ---------------- Pi 5 REST API ----------------

// Length of the UTF-8 sequence starting with `c` (1 for invalid lead bytes).
static size_t utf8_len(unsigned char c) {
    if (c >= 0xF0 && c <= 0xF4) return 4;
    if (c >= 0xE0) return c <= 0xEF ? 3 : 1;
    if (c >= 0xC2) return 2;
    return 1;
}

static bool put(char *dst, size_t dst_len, size_t &pos, const char *s, size_t n) {
    if (pos + n >= dst_len) return false;   // keep room for the NUL
    memcpy(dst + pos, s, n);
    pos += n;
    return true;
}

void ascii_sanitize(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0) return;
    size_t pos = 0;
    dst[0] = '\0';
    if (!src) return;
    for (const unsigned char *p = (const unsigned char *)src; *p;) {
        const unsigned char c = *p;
        bool fits;
        if (c < 0x80) {
            const char ch = (c < 0x20 || c == 0x7F) ? ' ' : (char)c;
            fits = put(dst, dst_len, pos, &ch, 1);
            p++;
        } else {
            size_t n = utf8_len(c);
            for (size_t i = 1; i < n; i++)
                if ((p[i] & 0xC0) != 0x80) { n = i; break; }   // truncated sequence
            if (n == 2 && c == 0xC2 && p[1] == 0xB0) fits = put(dst, dst_len, pos, "\xC2\xB0", 2);   // degree
            else if ((n == 2 && c == 0xC2 && p[1] == 0xB7) ||                                    // middle dot
                     (n == 3 && c == 0xE2 && p[1] == 0x80 && p[2] == 0xA2))                      // bullet
                fits = put(dst, dst_len, pos, "\xE2\x80\xA2", 3);
            else fits = put(dst, dst_len, pos, "?", 1);
            p += n;
        }
        if (!fits) break;
    }
    dst[pos] = '\0';
}

AlertLevel parse_alert_level(const char *s) {
    if (s && !strcmp(s, "critical")) return AlertLevel::Critical;
    if (s && !strcmp(s, "warning")) return AlertLevel::Warning;
    return AlertLevel::Info;
}

bool parse_alerts_summary(const char *json, size_t len, AlertSummary &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len) || !doc["unacked"].is<int>()) return false;
    const int unacked = doc["unacked"];
    out.unacked = (uint16_t)(unacked < 0 ? 0 : unacked > 65535 ? 65535 : unacked);
    const int critical = doc["critical"] | 0;
    out.critical = (uint16_t)(critical < 0 ? 0 : critical > 65535 ? 65535 : critical);
    out.latest_id = doc["latest_id"] | 0u;
    return true;
}

bool parse_alerts(const char *json, size_t len, AlertList &out) {
    // Only the fields we show: smaller document, unknown fields cost nothing.
    JsonDocument filter;
    for (const char *k : {"id", "level", "title", "detail", "ts", "count"}) filter["items"][0][k] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;
    JsonArray items = doc["items"];
    if (items.isNull()) return false;

    AlertList tmp{};
    for (JsonObject it : items) {
        if (tmp.count >= ALERTS_MAX) break;   // the Pi 5 caps at 10; never trust it
        const char *title = it["title"];
        if (!it["id"].is<uint32_t>() || !title || !*title) continue;
        Alert &a = tmp.items[tmp.count++];
        a.id = it["id"];
        a.level = parse_alert_level(it["level"]);
        ascii_sanitize(a.title, sizeof a.title, title);
        ascii_sanitize(a.detail, sizeof a.detail, it["detail"] | "");
        a.ts = it["ts"] | 0u;
        const int count = it["count"] | 1;
        a.count = (uint16_t)(count < 1 ? 1 : count > 65535 ? 65535 : count);
    }
    out = tmp;
    return true;
}

bool parse_location(const char *json, size_t len, Location &out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;
    const char *city = doc["city"];
    if (!city || !*city || !doc["lat"].is<float>() || !doc["lon"].is<float>()) return false;
    const float lat = doc["lat"], lon = doc["lon"];
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
    Location tmp{};
    ascii_sanitize(tmp.city, sizeof tmp.city, city);
    tmp.lat = lat;
    tmp.lon = lon;
    out = tmp;
    return true;
}

void format_age(char *buf, size_t buf_len, uint32_t ts, uint32_t now) {
    if (!buf || buf_len == 0) return;
    buf[0] = '\0';
    if (now < 1704067200u || ts == 0 || ts > now + 60) return;   // clock not set (< 2024) / future
    const uint32_t s = now > ts ? now - ts : 0;
    if (s < 60) snprintf(buf, buf_len, "now");
    else if (s < 3600) snprintf(buf, buf_len, "%u min", (unsigned)(s / 60));
    else if (s < 86400) snprintf(buf, buf_len, "%u h", (unsigned)(s / 3600));
    else snprintf(buf, buf_len, "%u d", (unsigned)(s / 86400));
}

bool build_wifi_qr(char *buf, size_t buf_len, const char *ssid, const char *pass) {
    if (!buf || buf_len == 0) return false;
    size_t pos = 0;
    buf[0] = '\0';
    const bool open = !pass || !*pass;
    return qr_append(buf, buf_len, pos, open ? "WIFI:T:nopass;S:" : "WIFI:T:WPA;S:", false) &&
           qr_append(buf, buf_len, pos, ssid, true) &&
           (open || (qr_append(buf, buf_len, pos, ";P:", false) && qr_append(buf, buf_len, pos, pass, true))) &&
           qr_append(buf, buf_len, pos, ";;", false);
}
