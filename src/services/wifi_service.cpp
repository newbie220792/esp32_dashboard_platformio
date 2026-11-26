#include "services/wifi_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <string.h>

#include "app_config.h"
#include "core/config.h"
#include "core/parsers.h"
#include "core/state.h"

// Boot-time Wi-Fi flow (task 3), then reconnect supervision. Every setup
// state change goes to the state store (StateGroup::WifiSetup).
//
//   CONNECTING ──ok──────────────────────────────────────────▶ CONNECTED
//      │ secrets.h first, then the network saved in NVS
//      ▼ fail
//   FAILED_SAVED ──Retry──▶ CONNECTING
//      │ 5 s countdown or "Setup now"
//      ▼
//   PORTAL (AP ESP32-Setup + captive portal, WiFiManager non-blocking)
//      │ user submits SSID/password
//      ▼
//   CONNECTING_NEW ──ok──▶ save to NVS, stop AP ──▶ CONNECTED
//      │ fail
//      ▼
//   FAILED_NEW (portal stays open, Attempt N) ──submit again──▶ CONNECTING_NEW

#define BIT_CONNECTED BIT0

static EventGroupHandle_t s_eg;
static QueueHandle_t s_cmd_queue;
static volatile uint8_t s_last_reason = 0;

struct Cred {
    char ssid[33];
    char pass[65];
};

// ---------------- helpers ----------------

static void post_state(WifiSetupState st, const char *ssid, WifiFailReason reason = WifiFailReason::None,
                       uint16_t attempt = 0) {
    WifiSetupInfo info{};
    info.state = st;
    info.reason = reason;
    info.attempt = attempt;
    strncpy(info.ssid, ssid ? ssid : "", sizeof info.ssid - 1);
    state_set_wifi_setup(info);   // a store, so the UI cannot miss a change
}

static void copy_cred(Cred &c, const char *ssid, const char *pass);

static bool load_saved(Cred &c) {
    return config_load_saved_wifi(c.ssid, sizeof c.ssid, c.pass, sizeof c.pass);
}

static void save_cred(const Cred &c) { config_save_wifi(c.ssid, c.pass); }

static WifiFailReason current_fail_reason() {
    return wifi_fail_reason(s_last_reason, (int)WiFi.status());
}

// One network, up to WIFI_CONNECT_TIMEOUT_MS. Gives up early once the same
// definitive failure (not found / wrong password) has been seen 3 times.
static bool try_connect(const Cred &c, WifiFailReason &reason) {
    Serial.printf("[wifi] trying '%s'\n", c.ssid);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    s_last_reason = 0;
    WiFi.begin(c.ssid, c.pass);

    const uint32_t start = millis();
    uint8_t seen_reason = 0, repeats = 0;
    while (millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        if (WiFi.status() == WL_CONNECTED) return true;
        uint8_t r = s_last_reason;
        if (r) {
            s_last_reason = 0;
            repeats = (r == seen_reason) ? repeats + 1 : 1;
            seen_reason = r;
            WifiFailReason fr = wifi_fail_reason(r, 0);
            if (repeats >= 3 && (fr == WifiFailReason::NotFound || fr == WifiFailReason::WrongPassword)) break;
            WiFi.reconnect();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    reason = seen_reason ? wifi_fail_reason(seen_reason, (int)WiFi.status()) : WifiFailReason::Timeout;
    Serial.printf("[wifi] '%s' failed: %s\n", c.ssid, wifi_fail_reason_text(reason));
    WiFi.disconnect();
    return false;
}

// Drop stale button presses; report whether a BOOT reset was requested.
static bool drain_commands() {
    WifiCmd cmd;
    bool reset = false;
    while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) reset |= (cmd == WifiCmd::ResetAndPortal);
    return reset;
}

// ---------------- portal ----------------

static WiFiManager *s_wm;
static bool s_submitted;
static Cred s_new;
static uint16_t s_attempt;

static void on_presave() {
    // Runs inside wm.process() just before WiFiManager tries the new network.
    String ssid = s_wm->server->arg("s");
    String pass = s_wm->server->arg("p");
    if (ssid.isEmpty()) return;
    copy_cred(s_new, ssid.c_str(), pass.c_str());
    s_submitted = true;
    s_attempt++;
    s_last_reason = 0;
    post_state(WifiSetupState::ConnectingNew, s_new.ssid, WifiFailReason::None, s_attempt);
}

static void run_portal(Cred &out) {
    if (!s_wm) {
        s_wm = new WiFiManager();
        s_wm->setDebugOutput(false);
        s_wm->setConfigPortalBlocking(false);
        s_wm->setBreakAfterConfig(false);       // keep the portal open after a failed attempt
        s_wm->setConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
        s_wm->setSaveConnectTimeout(WIFI_CONNECT_TIMEOUT_MS / 1000);
        s_wm->setCaptivePortalEnable(true);
        s_wm->setShowInfoUpdate(false);
        s_wm->setTitle("ESP32 Dashboard");
        s_wm->setPreSaveConfigCallback(on_presave);
    }
    s_attempt = 0;
    s_submitted = false;
    const char *ap_pass = strlen(config_setup_ap_password()) ? config_setup_ap_password() : nullptr;
    s_wm->startConfigPortal(SETUP_AP_SSID, ap_pass);
    Serial.printf("[wifi] portal open: %s @ %s\n", SETUP_AP_SSID, SETUP_AP_IP);
    post_state(WifiSetupState::Portal, SETUP_AP_SSID);
    drain_commands();

    for (;;) {
        s_submitted = false;
        // When a form was submitted, this call also runs the (blocking) connect
        // attempt; the LVGL task keeps animating meanwhile.
        bool ok = s_wm->process();
        if (ok || WiFi.status() == WL_CONNECTED) {
            if (!s_submitted && s_new.ssid[0] == '\0') {
                // Connected in the background without a submit: use what the radio has.
                copy_cred(s_new, WiFi.SSID().c_str(), WiFi.psk().c_str());
            }
            break;
        }
        if (s_submitted) {
            WifiFailReason r = current_fail_reason();
            if (r == WifiFailReason::None) r = WifiFailReason::Timeout;
            Serial.printf("[wifi] portal attempt %u for '%s' failed: %s\n", s_attempt, s_new.ssid,
                          wifi_fail_reason_text(r));
            WiFi.disconnect(false, false);   // stop STA retries; AP stays up
            post_state(WifiSetupState::FailedNew, s_new.ssid, r, s_attempt);
        }
        drain_commands();   // buttons don't apply here; the portal is already open
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    out = s_new;
    save_cred(out);
    s_wm->stopConfigPortal();
    WiFi.mode(WIFI_STA);
    memset(&s_new, 0, sizeof s_new);
}

static bool take_command(WifiCmd &cmd) {
    return s_cmd_queue && xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE;
}

static void note_disconnect(uint8_t reason) { s_last_reason = reason ? reason : 1; }

static void copy_cred(Cred &c, const char *ssid, const char *pass) {
    strncpy(c.ssid, ssid, sizeof c.ssid - 1);
    c.ssid[sizeof c.ssid - 1] = '\0';
    strncpy(c.pass, pass, sizeof c.pass - 1);
    c.pass[sizeof c.pass - 1] = '\0';
}

// Blocks the wifi task until a network is connected. With `force_portal`, skips
// straight to the portal. Writes the credentials that worked into ssid/pass.
static void setup_run(bool force_portal, char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    Cred used{};
    bool connected = false;
    WiFi.setAutoReconnect(false);   // we drive retries ourselves during setup

    while (!force_portal && !connected) {
        // secrets.h always wins; the network saved via the portal is the fallback.
        Cred list[2];
        int n = 0;
        copy_cred(list[n++], config_wifi_ssid(), config_wifi_password());
        Cred saved{};
        if (load_saved(saved) && (strcmp(saved.ssid, list[0].ssid) || strcmp(saved.pass, list[0].pass)))
            list[n++] = saved;

        WifiFailReason secrets_reason = WifiFailReason::Timeout;
        for (int i = 0; i < n && !connected; i++) {
            WifiFailReason r;
            post_state(WifiSetupState::Connecting, list[i].ssid);
            if (try_connect(list[i], r)) {
                connected = true;
                used = list[i];
            } else if (i == 0) {
                secrets_reason = r;
            }
        }
        if (connected) break;

        // Screen 4 reports the secrets.h network; wait for Retry / Setup now.
        WifiCmd cmd;
        if (drain_commands()) {          // BOOT held while we were connecting
            config_erase_saved_wifi();
            break;
        }
        post_state(WifiSetupState::FailedSaved, list[0].ssid, secrets_reason);
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(WIFI_FAIL_COUNTDOWN_MS)) != pdTRUE)
            cmd = WifiCmd::SetupNow;   // countdown expired
        if (cmd == WifiCmd::Retry) continue;
        if (cmd == WifiCmd::ResetAndPortal) config_erase_saved_wifi();
        break;
    }

    if (!connected) run_portal(used);

    WiFi.setAutoReconnect(true);
    strncpy(ssid, used.ssid, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    strncpy(pass, used.pass, pass_len - 1);
    pass[pass_len - 1] = '\0';
    Serial.printf("[wifi] connected to '%s'\n", ssid);
    post_state(WifiSetupState::Connected, ssid);
}

// ---------------- supervision ----------------

// The event callbacks only give a fast first update. The wifi task re-reads the
// real link state every 250 ms (wifi_sync_link), so a missed or out-of-order
// event (e.g. a DISCONNECTED arriving after GOT_IP while the portal shuts down)
// can't leave BIT_CONNECTED or the status-bar icon stuck.

// Runs on the default esp_event loop task: only touch FreeRTOS primitives.
static void on_got_ip(void *, esp_event_base_t, int32_t, void *data) {
    auto *info = static_cast<ip_event_got_ip_t *>(data);
    xEventGroupSetBits(s_eg, BIT_CONNECTED);
    state_set_wifi(true, 0, info ? info->ip_info.ip.addr : 0);
}

static void on_disconnected(void *, esp_event_base_t, int32_t, void *data) {
    xEventGroupClearBits(s_eg, BIT_CONNECTED);
    state_set_wifi(false, 0, 0);
    auto *info = static_cast<wifi_event_sta_disconnected_t *>(data);
    note_disconnect(info ? info->reason : 0);
}

// Some routers hand out no (or a dead) DNS server: add public fallbacks for
// the slots DHCP left empty.
static void ensure_dns() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return;
    const struct { esp_netif_dns_type_t type; uint32_t addr; } fallbacks[] = {
        {ESP_NETIF_DNS_MAIN, ESP_IP4TOADDR(8, 8, 8, 8)},
        {ESP_NETIF_DNS_BACKUP, ESP_IP4TOADDR(1, 1, 1, 1)},
    };
    for (const auto &f : fallbacks) {
        esp_netif_dns_info_t dns{};
        if (esp_netif_get_dns_info(netif, f.type, &dns) == ESP_OK && dns.ip.u_addr.ip4.addr != 0) continue;
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = f.addr;
        esp_netif_set_dns_info(netif, f.type, &dns);
    }
}

// Level-triggered link check (wifi task only).
static void wifi_sync_link() {
    static bool last_up = false;
    static uint32_t last_rssi_ms = 0;
    const IPAddress ip = WiFi.localIP();
    const bool up = WiFi.status() == WL_CONNECTED && (uint32_t)ip != 0;

    if (up) xEventGroupSetBits(s_eg, BIT_CONNECTED);
    else xEventGroupClearBits(s_eg, BIT_CONNECTED);

    if (up != last_up || millis() - last_rssi_ms > 2000) {
        state_set_wifi(up, up ? (int8_t)WiFi.RSSI() : 0, (uint32_t)ip);
        last_rssi_ms = millis();
    }
    if (up != last_up) {
        if (up) ensure_dns();
        Serial.printf("[wifi] link %s: status=%d ip=%s dns=%s/%s rssi=%d\n", up ? "UP" : "DOWN",
                      (int)WiFi.status(), ip.toString().c_str(), WiFi.dnsIP(0).toString().c_str(),
                      WiFi.dnsIP(1).toString().c_str(), up ? (int)WiFi.RSSI() : 0);
        last_up = up;
    }
}

void wifi_service_task(void *) {
    char ssid[33], pass[65];
    setup_run(false, ssid, sizeof ssid, pass, sizeof pass);
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());

    uint32_t down_since = millis();
    for (;;) {
        WifiCmd cmd;
        if (take_command(cmd) && cmd == WifiCmd::ResetAndPortal) {
            // BOOT held: forget the saved network and reconfigure.
            config_erase_saved_wifi();
            WiFi.disconnect();
            setup_run(true, ssid, sizeof ssid, pass, sizeof pass);
        }

        wifi_sync_link();
        if (wifi_service_is_connected()) {
            down_since = millis();
        } else if (millis() - down_since > 20000) {
            // Arduino's auto-reconnect occasionally gives up; kick it.
            Serial.println("[wifi] reconnecting...");
            WiFi.disconnect();
            WiFi.begin(ssid, pass);
            down_since = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// ---------------- public ----------------

void wifi_service_init() {
    s_eg = xEventGroupCreate();
    s_cmd_queue = xQueueCreate(4, sizeof(WifiCmd));
    WiFi.persistent(false);         // credentials live in our own NVS namespace (core/config)
    WiFi.mode(WIFI_STA);            // also creates netif + default event loop
    WiFi.setSleep(false);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, nullptr, nullptr);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnected, nullptr, nullptr);
}

bool wifi_service_command(WifiCmd cmd) {
    return s_cmd_queue && xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE;
}

bool wifi_service_is_connected() {
    return s_eg && (xEventGroupGetBits(s_eg) & BIT_CONNECTED);
}

bool wifi_service_wait_connected(TickType_t timeout) {
    return xEventGroupWaitBits(s_eg, BIT_CONNECTED, pdFALSE, pdTRUE, timeout) & BIT_CONNECTED;
}
