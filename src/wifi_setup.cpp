#include "wifi_setup.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <freertos/queue.h>
#include <string.h>

#include "app_config.h"
#include "app_events.h"
#include "parsers.h"
#include "secrets.h"

#ifndef SETUP_AP_PASSWORD
#define SETUP_AP_PASSWORD ""   // see secrets.example.h
#endif
static_assert(sizeof(SETUP_AP_PASSWORD) == 1 || sizeof(SETUP_AP_PASSWORD) >= 9,
              "SETUP_AP_PASSWORD must be empty or at least 8 characters");

#define NVS_NAMESPACE "wifi_cfg"

static QueueHandle_t s_cmd_queue;
static volatile uint8_t s_last_reason = 0;

struct Cred {
    char ssid[33];
    char pass[65];
};

// ---------------- helpers ----------------

static void post_state(WifiSetupState st, const char *ssid, WifiFailReason reason = WifiFailReason::None,
                       uint16_t attempt = 0) {
    AppEvent ev{};
    ev.type = AppEventType::WifiSetup;
    ev.wifi.state = st;
    ev.wifi.reason = reason;
    ev.wifi.attempt = attempt;
    strncpy(ev.wifi.ssid, ssid ? ssid : "", sizeof ev.wifi.ssid - 1);
    // State changes must not be lost; the UI drains quickly.
    for (int i = 0; i < 50 && !app_events_post(ev); i++) vTaskDelay(pdMS_TO_TICKS(20));
}

static void copy_cred(Cred &c, const char *ssid, const char *pass);

static bool load_saved(Cred &c) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return false;   // namespace absent = nothing saved
    String ssid = p.getString("ssid", "");
    String pass = p.getString("pass", "");
    p.end();
    if (ssid.isEmpty()) return false;
    copy_cred(c, ssid.c_str(), pass.c_str());
    return true;
}

static void save_cred(const Cred &c) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.putString("ssid", c.ssid);
    p.putString("pass", c.pass);
    p.end();
    Serial.printf("[wifi] saved '%s' to NVS\n", c.ssid);
}

void wifi_setup_erase_saved() {
    Preferences p;
    if (p.begin(NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
    Serial.println("[wifi] saved network erased");
}

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
    const char *ap_pass = strlen(SETUP_AP_PASSWORD) ? SETUP_AP_PASSWORD : nullptr;
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

// ---------------- public ----------------

void wifi_setup_init() {
    s_cmd_queue = xQueueCreate(4, sizeof(WifiCmd));
}

bool wifi_setup_command(WifiCmd cmd) {
    return s_cmd_queue && xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE;
}

bool wifi_setup_take_command(WifiCmd &cmd) {
    return s_cmd_queue && xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE;
}

void wifi_setup_note_disconnect(unsigned char reason) {
    s_last_reason = reason ? reason : 1;
}

static void copy_cred(Cred &c, const char *ssid, const char *pass) {
    strncpy(c.ssid, ssid, sizeof c.ssid - 1);
    c.ssid[sizeof c.ssid - 1] = '\0';
    strncpy(c.pass, pass, sizeof c.pass - 1);
    c.pass[sizeof c.pass - 1] = '\0';
}

void wifi_setup_run(bool force_portal, char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    Cred used{};
    bool connected = false;
    WiFi.setAutoReconnect(false);   // we drive retries ourselves during setup

    while (!force_portal && !connected) {
        // secrets.h always wins; the network saved via the portal is the fallback.
        Cred list[2];
        int n = 0;
        copy_cred(list[n++], WIFI_SSID, WIFI_PASSWORD);
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
            wifi_setup_erase_saved();
            break;
        }
        post_state(WifiSetupState::FailedSaved, list[0].ssid, secrets_reason);
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(WIFI_FAIL_COUNTDOWN_MS)) != pdTRUE)
            cmd = WifiCmd::SetupNow;   // countdown expired
        if (cmd == WifiCmd::Retry) continue;
        if (cmd == WifiCmd::ResetAndPortal) wifi_setup_erase_saved();
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
