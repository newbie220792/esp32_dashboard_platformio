#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "parsers.h"

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

int main() {
    UNITY_BEGIN();
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
    return UNITY_END();
}
