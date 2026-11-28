#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "core/parsers.h"

void setUp() {}
void tearDown() {}

static bool services(const char *json, ServiceList &out) { return parse_services(json, strlen(json), out); }
static bool home(const char *json, HomeConfig &out) { return parse_home_config(json, strlen(json), out); }

// ---------------- pi5/stats ----------------
static void test_pi_stats() {
    const char *j = R"({"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58})";
    PiStats s{};
    TEST_ASSERT_TRUE(parse_pi_stats(j, strlen(j), s));
    TEST_ASSERT_EQUAL_FLOAT(37, s.cpu);
    TEST_ASSERT_EQUAL_FLOAT(4.1f, s.ram_used);
    TEST_ASSERT_EQUAL_FLOAT(58, s.temp);
    TEST_ASSERT_FALSE(parse_pi_stats("{bad", 4, s));
}

// ---------------- pi5/services ----------------
static void test_services_spec_example() {
    ServiceList l{};
    TEST_ASSERT_TRUE(services(R"({"services":[
        {"name":"nginx","status":"running","port":80},
        {"name":"mosquitto","status":"running","port":1883},
        {"name":"docker","status":"running","port":null},
        {"name":"node-red","status":"stopped","port":1880}]})", l));
    TEST_ASSERT_EQUAL(4, l.count);
    TEST_ASSERT_EQUAL_STRING("nginx", l.items[0].name);
    TEST_ASSERT_TRUE(l.items[0].running);
    TEST_ASSERT_EQUAL(80, l.items[0].port);
    TEST_ASSERT_EQUAL(-1, l.items[2].port);
    TEST_ASSERT_FALSE(l.items[3].running);
}

static void test_services_unknown_status_is_stopped() {
    ServiceList l{};
    TEST_ASSERT_TRUE(services(R"({"services":[{"name":"x","status":"failed"},{"name":"y"}]})", l));
    TEST_ASSERT_EQUAL(2, l.count);
    TEST_ASSERT_FALSE(l.items[0].running);
    TEST_ASSERT_FALSE(l.items[1].running);
    TEST_ASSERT_EQUAL(-1, l.items[1].port);
}

static void test_services_bad_json_keeps_old_list() {
    ServiceList l{};
    services(R"({"services":[{"name":"keep","status":"running","port":1}]})", l);
    TEST_ASSERT_FALSE(services(R"({"services":[{"name":"x")", l));
    TEST_ASSERT_FALSE(services(R"({"other":1})", l));
    TEST_ASSERT_EQUAL(1, l.count);
    TEST_ASSERT_EQUAL_STRING("keep", l.items[0].name);
}

static void test_services_capped_and_truncated() {
    char buf[1024] = "{\"services\":[";
    for (int i = 0; i < 15; i++) {
        char item[64];
        snprintf(item, sizeof item, "%s{\"name\":\"service-with-a-very-long-name-%d\",\"status\":\"running\"}",
                 i ? "," : "", i);
        strcat(buf, item);
    }
    strcat(buf, "]}");
    ServiceList l{};
    TEST_ASSERT_TRUE(services(buf, l));
    TEST_ASSERT_EQUAL(MAX_SERVICES, l.count);
    TEST_ASSERT_EQUAL(SERVICE_NAME_LEN - 1, strlen(l.items[0].name));
}

// ---------------- home/config ----------------
static const char *kHomeExample = R"({"rooms":[{"id":"living","name":"Living room","panels":[
  {"id":"a","name":"Panel A","switches":[
    {"id":1,"name":"Ceiling","icon":"bulb","state":"ON"},
    {"id":2,"name":"Wall","icon":"bulb","state":"OFF"},
    {"id":3,"name":"Fan","icon":"fan","state":"ON"}]},
  {"id":"b","name":"Panel B","switches":[
    {"id":1,"name":"AC","icon":"snow","state":"OFF"},
    {"id":2,"name":"TV","icon":"plug","state":"OFF"},
    {"id":3,"name":"Lamp","icon":"bulb","state":"ON"}]}]}]})";

static void test_home_spec_example() {
    static HomeConfig c;
    TEST_ASSERT_TRUE(home(kHomeExample, c));
    TEST_ASSERT_EQUAL(1, c.room_count);
    const HomeRoom &r = c.rooms[0];
    TEST_ASSERT_EQUAL_STRING("living", r.id);
    TEST_ASSERT_EQUAL_STRING("Living room", r.name);
    TEST_ASSERT_EQUAL(2, r.panel_count);
    TEST_ASSERT_EQUAL_STRING("Panel B", r.panels[1].name);
    TEST_ASSERT_TRUE(r.panels[0].switches[0].on);
    TEST_ASSERT_FALSE(r.panels[0].switches[1].on);
    TEST_ASSERT_EQUAL((int)SwitchIcon::Fan, (int)r.panels[0].switches[2].icon);
    TEST_ASSERT_EQUAL((int)SwitchIcon::Snow, (int)r.panels[1].switches[0].icon);
    TEST_ASSERT_EQUAL((int)SwitchIcon::Plug, (int)r.panels[1].switches[1].icon);
    TEST_ASSERT_EQUAL(3, r.panels[1].switches[2].id);
}

static void test_home_pads_and_truncates_switches() {
    static HomeConfig c;
    TEST_ASSERT_TRUE(home(R"({"rooms":[{"id":"k","panels":[
        {"id":"p1","switches":[{"name":"One"}]},
        {"id":"p2","switches":[{"id":1},{"id":2},{"id":3},{"id":4}]}]}]})", c));
    const HomeRoom &r = c.rooms[0];
    TEST_ASSERT_EQUAL_STRING("k", r.name);              // name falls back to id
    TEST_ASSERT_TRUE(r.panels[0].switches[0].present);
    TEST_ASSERT_EQUAL(1, r.panels[0].switches[0].id);   // id falls back to index+1
    TEST_ASSERT_FALSE(r.panels[0].switches[1].present);
    TEST_ASSERT_FALSE(r.panels[0].switches[2].present);
    TEST_ASSERT_EQUAL(3, r.panels[1].switches[2].id);   // 4th dropped
}

static void test_home_skips_unsafe_ids() {
    static HomeConfig c;
    TEST_ASSERT_TRUE(home(R"({"rooms":[{"id":"a/b"},{"id":"+"},{"name":"no id"},{"id":"ok","panels":[{"id":"#"}]}]})", c));
    TEST_ASSERT_EQUAL(1, c.room_count);
    TEST_ASSERT_EQUAL_STRING("ok", c.rooms[0].id);
    TEST_ASSERT_EQUAL(0, c.rooms[0].panel_count);
}

static void test_home_bad_json_keeps_old() {
    static HomeConfig c;
    home(kHomeExample, c);
    TEST_ASSERT_FALSE(home("{\"rooms\":[", c));
    TEST_ASSERT_FALSE(home("{}", c));
    TEST_ASSERT_EQUAL(1, c.room_count);
    TEST_ASSERT_EQUAL(2, c.rooms[0].panel_count);
}

// ---------------- switch topics ----------------
static void test_switch_state_topic() {
    SwitchState s{};
    TEST_ASSERT_TRUE(parse_switch_state("home/living/a/3/state", "ON", 2, s));
    TEST_ASSERT_EQUAL_STRING("living", s.room);
    TEST_ASSERT_EQUAL_STRING("a", s.panel);
    TEST_ASSERT_EQUAL(3, s.id);
    TEST_ASSERT_TRUE(s.on);
    TEST_ASSERT_TRUE(parse_switch_state("home/living/a/3/state", "OFF", 3, s));
    TEST_ASSERT_FALSE(s.on);
}

static void test_switch_state_rejects() {
    SwitchState s{};
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/3/set", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/state", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/3/state/x", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home//a/3/state", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/x3/state", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/3/state", "on", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/living/a/3/state", "ONX", 3, s));
    TEST_ASSERT_FALSE(parse_switch_state("pi5/living/a/3/state", "ON", 2, s));
    TEST_ASSERT_FALSE(parse_switch_state("home/a-room-id-that-is-too-long/a/3/state", "ON", 2, s));
}

static void test_set_topic() {
    char buf[64];
    TEST_ASSERT_TRUE(build_switch_set_topic(buf, sizeof buf, "living", "a", 2));
    TEST_ASSERT_EQUAL_STRING("home/living/a/2/set", buf);
    char small[10];
    TEST_ASSERT_FALSE(build_switch_set_topic(small, sizeof small, "living", "a", 2));
}

static void test_topic_matches() {
    TEST_ASSERT_TRUE(mqtt_topic_matches("pi5/stats", "pi5/stats"));
    TEST_ASSERT_FALSE(mqtt_topic_matches("pi5/stats", "pi5/services"));
    TEST_ASSERT_FALSE(mqtt_topic_matches("pi5/stats", "pi5/stats/x"));
    TEST_ASSERT_TRUE(mqtt_topic_matches("home/+/+/+/state", "home/living/a/1/state"));
    TEST_ASSERT_FALSE(mqtt_topic_matches("home/+/+/+/state", "home/config"));
    TEST_ASSERT_FALSE(mqtt_topic_matches("home/+/+/+/state", "home/living/a/1/set"));
    TEST_ASSERT_FALSE(mqtt_topic_matches("home/+/+/+/state", "home/living/a/state"));
    TEST_ASSERT_TRUE(mqtt_topic_matches("home/#", "home/living/a/1/state"));
}

static void test_wifi_fail_reason() {
    TEST_ASSERT_EQUAL((int)WifiFailReason::NotFound, (int)wifi_fail_reason(201, 6));
    TEST_ASSERT_EQUAL((int)WifiFailReason::WrongPassword, (int)wifi_fail_reason(15, 6));
    TEST_ASSERT_EQUAL((int)WifiFailReason::WrongPassword, (int)wifi_fail_reason(202, 6));
    TEST_ASSERT_EQUAL((int)WifiFailReason::WrongPassword, (int)wifi_fail_reason(204, 6));
    TEST_ASSERT_EQUAL((int)WifiFailReason::ConnectFailed, (int)wifi_fail_reason(203, 6));
    TEST_ASSERT_EQUAL((int)WifiFailReason::NotFound, (int)wifi_fail_reason(0, 1));      // WL_NO_SSID_AVAIL
    TEST_ASSERT_EQUAL((int)WifiFailReason::ConnectFailed, (int)wifi_fail_reason(0, 4)); // WL_CONNECT_FAILED
    TEST_ASSERT_EQUAL((int)WifiFailReason::Timeout, (int)wifi_fail_reason(0, 6));       // WL_DISCONNECTED
    TEST_ASSERT_EQUAL_STRING("Wrong password", wifi_fail_reason_text(WifiFailReason::WrongPassword));
}

static void test_wifi_qr() {
    char buf[96];
    TEST_ASSERT_TRUE(build_wifi_qr(buf, sizeof buf, "ESP32-Setup", ""));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:ESP32-Setup;;", buf);
    TEST_ASSERT_TRUE(build_wifi_qr(buf, sizeof buf, "ESP32-Setup", "secret123"));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:ESP32-Setup;P:secret123;;", buf);
    TEST_ASSERT_TRUE(build_wifi_qr(buf, sizeof buf, "a;b", "p:w\\d"));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b;P:p\\:w\\\\d;;", buf);
    char small[16];
    TEST_ASSERT_FALSE(build_wifi_qr(small, sizeof small, "ESP32-Setup", "secret123"));
}

// ---------------- Pi 5 REST API (task 8) ----------------
static void test_ascii_sanitize() {
    char b[32];
    ascii_sanitize(b, sizeof b, "TP Dong Nai");
    TEST_ASSERT_EQUAL_STRING("TP Dong Nai", b);
    ascii_sanitize(b, sizeof b, "TP \xC4\x90\xE1\xBB\x93ng Nai");   // "TP Đồng Nai"
    TEST_ASSERT_EQUAL_STRING("TP ??ng Nai", b);
    ascii_sanitize(b, sizeof b, "74\xC2\xB0" "C \xC2\xB7 limit");   // degree kept, middle dot -> bullet
    TEST_ASSERT_EQUAL_STRING("74\xC2\xB0" "C \xE2\x80\xA2 limit", b);
    ascii_sanitize(b, sizeof b, "a\tb\x01");
    TEST_ASSERT_EQUAL_STRING("a b ", b);
    ascii_sanitize(b, 4, "abcdef");
    TEST_ASSERT_EQUAL_STRING("abc", b);
    ascii_sanitize(b, 5, "ab\xC2\xB0\xC2\xB0");   // never splits a character
    TEST_ASSERT_EQUAL_STRING("ab\xC2\xB0", b);
    ascii_sanitize(b, sizeof b, "x\xE2\x80");     // truncated sequence at the end
    TEST_ASSERT_EQUAL_STRING("x?", b);
    ascii_sanitize(b, sizeof b, nullptr);
    TEST_ASSERT_EQUAL_STRING("", b);
}

static void test_alerts_summary() {
    AlertSummary s{};
    const char *j = R"({"unacked":3,"critical":1,"latest_id":128})";
    TEST_ASSERT_TRUE(parse_alerts_summary(j, strlen(j), s));
    TEST_ASSERT_EQUAL(3, s.unacked);
    TEST_ASSERT_EQUAL(1, s.critical);
    TEST_ASSERT_EQUAL(128, s.latest_id);
    TEST_ASSERT_FALSE(parse_alerts_summary(R"({"error":"bad token"})", 21, s));
    TEST_ASSERT_EQUAL(3, s.unacked);   // untouched
}

static void test_alerts_spec_example() {
    const char *j = R"({"items":[
        {"id":128,"level":"critical","title":"Pi 5 temperature high","detail":"74°C · limit 70°C","ts":1764000000,"count":3,"ack":false},
        {"id":127,"level":"warning","title":"node-red stopped","detail":"Port 1880","ts":1763999400,"count":1,"ack":false},
        {"id":126,"level":"rain","title":"odd level","ts":1}]})";
    AlertList l{};
    TEST_ASSERT_TRUE(parse_alerts(j, strlen(j), l));
    TEST_ASSERT_EQUAL(3, l.count);
    TEST_ASSERT_EQUAL(128, l.items[0].id);
    TEST_ASSERT_TRUE(l.items[0].level == AlertLevel::Critical);
    TEST_ASSERT_EQUAL_STRING("Pi 5 temperature high", l.items[0].title);
    TEST_ASSERT_EQUAL_STRING("74\xC2\xB0" "C \xE2\x80\xA2 limit 70\xC2\xB0" "C", l.items[0].detail);
    TEST_ASSERT_EQUAL(3, l.items[0].count);
    TEST_ASSERT_EQUAL_UINT32(1764000000u, l.items[0].ts);
    TEST_ASSERT_TRUE(l.items[1].level == AlertLevel::Warning);
    TEST_ASSERT_TRUE(l.items[2].level == AlertLevel::Info);   // unknown level
    TEST_ASSERT_EQUAL_STRING("", l.items[2].detail);
    TEST_ASSERT_EQUAL(1, l.items[2].count);
}

static void test_alerts_capped_and_bad_json() {
    char buf[2048] = "{\"items\":[";
    for (int i = 0; i < 14; i++) {
        char item[96];
        snprintf(item, sizeof item, "%s{\"id\":%d,\"level\":\"info\",\"title\":\"t%d\",\"ts\":1}", i ? "," : "", 100 - i, i);
        strcat(buf, item);
    }
    strcat(buf, "]}");
    AlertList l{};
    TEST_ASSERT_TRUE(parse_alerts(buf, strlen(buf), l));
    TEST_ASSERT_EQUAL(ALERTS_MAX, l.count);
    TEST_ASSERT_EQUAL(100, l.items[0].id);

    TEST_ASSERT_FALSE(parse_alerts(R"({"items":[{"id":1)", 17, l));
    TEST_ASSERT_FALSE(parse_alerts(R"({"error":"x"})", 13, l));
    TEST_ASSERT_EQUAL(ALERTS_MAX, l.count);   // untouched

    const char *skip = R"({"items":[{"title":"no id"},{"id":5,"title":""},{"id":6,"title":"ok"}]})";
    TEST_ASSERT_TRUE(parse_alerts(skip, strlen(skip), l));
    TEST_ASSERT_EQUAL(1, l.count);
    TEST_ASSERT_EQUAL(6, l.items[0].id);
}

static void test_location() {
    Location loc{};
    const char *j = R"({"city":"TP Đồng Nai","lat":10.9574,"lon":106.8427})";
    TEST_ASSERT_TRUE(parse_location(j, strlen(j), loc));
    TEST_ASSERT_EQUAL_STRING("TP ??ng Nai", loc.city);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.9574f, loc.lat);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 106.8427f, loc.lon);
    const char *bad[] = {R"({"city":"","lat":1,"lon":2})", R"({"city":"X","lat":1})",
                         R"({"city":"X","lat":91,"lon":2})", R"({"city":"X","lat":"1","lon":2})", "{"};
    for (const char *b : bad) TEST_ASSERT_FALSE(parse_location(b, strlen(b), loc));
    TEST_ASSERT_EQUAL_STRING("TP ??ng Nai", loc.city);   // untouched
}

static void test_format_age() {
    char b[16];
    const uint32_t now = 1764000000u;
    format_age(b, sizeof b, now - 10, now);
    TEST_ASSERT_EQUAL_STRING("now", b);
    format_age(b, sizeof b, now - 120, now);
    TEST_ASSERT_EQUAL_STRING("2 min", b);
    format_age(b, sizeof b, now - 3 * 3600, now);
    TEST_ASSERT_EQUAL_STRING("3 h", b);
    format_age(b, sizeof b, now - 2 * 86400, now);
    TEST_ASSERT_EQUAL_STRING("2 d", b);
    format_age(b, sizeof b, now - 10, 1000);   // clock not synced
    TEST_ASSERT_EQUAL_STRING("", b);
    format_age(b, sizeof b, now + 600, now);   // from the future
    TEST_ASSERT_EQUAL_STRING("", b);
}

// ---------------- System info (task 9) ----------------
static void test_rssi_bars() {
    TEST_ASSERT_EQUAL(4, rssi_bars(-40));
    TEST_ASSERT_EQUAL(4, rssi_bars(-55));
    TEST_ASSERT_EQUAL(3, rssi_bars(-56));
    TEST_ASSERT_EQUAL(3, rssi_bars(-65));
    TEST_ASSERT_EQUAL(2, rssi_bars(-66));
    TEST_ASSERT_EQUAL(2, rssi_bars(-75));
    TEST_ASSERT_EQUAL(1, rssi_bars(-76));
    TEST_ASSERT_EQUAL(1, rssi_bars(-85));
    TEST_ASSERT_EQUAL(0, rssi_bars(-86));
    TEST_ASSERT_EQUAL(0, rssi_bars(0));   // not connected
}

static void test_format_uptime() {
    char b[24];
    format_uptime(b, sizeof b, 59);
    TEST_ASSERT_EQUAL_STRING("0m", b);
    format_uptime(b, sizeof b, 12 * 60 + 5);
    TEST_ASSERT_EQUAL_STRING("12m", b);
    format_uptime(b, sizeof b, 4 * 3600 + 12 * 60);
    TEST_ASSERT_EQUAL_STRING("4h 12m", b);
    format_uptime(b, sizeof b, 2 * 86400 + 4 * 3600 + 12 * 60);
    TEST_ASSERT_EQUAL_STRING("2d 4h 12m", b);
    format_uptime(b, sizeof b, 60ULL * 86400);   // past the 49-day millis() wrap
    TEST_ASSERT_EQUAL_STRING("60d 0h 0m", b);
}

static void test_format_size() {
    char b[32];
    format_size(b, sizeof b, 182 * 1024 + 500);
    TEST_ASSERT_EQUAL_STRING("182 KB", b);
    format_size(b, sizeof b, 1551892);   // 1.48 MB
    TEST_ASSERT_EQUAL_STRING("1.48 MB", b);
    format_size_pair(b, sizeof b, 182 * 1024, 320 * 1024);
    TEST_ASSERT_EQUAL_STRING("182 / 320 KB", b);
    format_size_pair(b, sizeof b, 7444889, 8386279);   // PSRAM: 7.1 / 8 MB
    TEST_ASSERT_EQUAL_STRING("7.1 / 8 MB", b);
    format_size_pair(b, sizeof b, 1698693, 0x1F0000);   // OTA slot: 1.62 / 1.94 MB
    TEST_ASSERT_EQUAL_STRING("1.62 / 1.94 MB", b);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_sanitize);
    RUN_TEST(test_alerts_summary);
    RUN_TEST(test_alerts_spec_example);
    RUN_TEST(test_alerts_capped_and_bad_json);
    RUN_TEST(test_location);
    RUN_TEST(test_format_age);
    RUN_TEST(test_rssi_bars);
    RUN_TEST(test_format_uptime);
    RUN_TEST(test_format_size);
    RUN_TEST(test_pi_stats);
    RUN_TEST(test_services_spec_example);
    RUN_TEST(test_services_unknown_status_is_stopped);
    RUN_TEST(test_services_bad_json_keeps_old_list);
    RUN_TEST(test_services_capped_and_truncated);
    RUN_TEST(test_home_spec_example);
    RUN_TEST(test_home_pads_and_truncates_switches);
    RUN_TEST(test_home_skips_unsafe_ids);
    RUN_TEST(test_home_bad_json_keeps_old);
    RUN_TEST(test_switch_state_topic);
    RUN_TEST(test_switch_state_rejects);
    RUN_TEST(test_set_topic);
    RUN_TEST(test_topic_matches);
    RUN_TEST(test_wifi_fail_reason);
    RUN_TEST(test_wifi_qr);
    return UNITY_END();
}
