#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include "app_config.h"
#include "net_state.h"
#include "wifi_setup.h"

#define BIT_CONNECTED BIT0

static EventGroupHandle_t s_eg;

// The event callbacks only give a fast first update. The wifi task re-reads the
// real link state every 250 ms (wifi_sync_link), so a missed or out-of-order
// event (e.g. a DISCONNECTED arriving after GOT_IP while the portal shuts down)
// can't leave BIT_CONNECTED or the status-bar icon stuck.

// Runs on the default esp_event loop task: only touch FreeRTOS primitives.
static void on_got_ip(void *, esp_event_base_t, int32_t, void *data) {
    auto *info = static_cast<ip_event_got_ip_t *>(data);
    xEventGroupSetBits(s_eg, BIT_CONNECTED);
    net_state_set_wifi(true, 0, info ? info->ip_info.ip.addr : 0);
}

static void on_disconnected(void *, esp_event_base_t, int32_t, void *data) {
    xEventGroupClearBits(s_eg, BIT_CONNECTED);
    net_state_set_wifi(false, 0, 0);
    auto *info = static_cast<wifi_event_sta_disconnected_t *>(data);
    wifi_setup_note_disconnect(info ? info->reason : 0);
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
        net_state_set_wifi(up, up ? (int8_t)WiFi.RSSI() : 0, (uint32_t)ip);
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

static void wifi_task(void *) {
    char ssid[33], pass[65];
    wifi_setup_run(false, ssid, sizeof ssid, pass, sizeof pass);
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
    Serial.printf("[wifi] ip=%s\n", WiFi.localIP().toString().c_str());

    uint32_t down_since = millis();
    for (;;) {
        WifiCmd cmd;
        if (wifi_setup_take_command(cmd) && cmd == WifiCmd::ResetAndPortal) {
            // BOOT held: forget the saved network and reconfigure.
            wifi_setup_erase_saved();
            WiFi.disconnect();
            wifi_setup_run(true, ssid, sizeof ssid, pass, sizeof pass);
        }

        wifi_sync_link();
        if (wifi_is_connected()) {
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

void wifi_start() {
    s_eg = xEventGroupCreate();
    wifi_setup_init();
    WiFi.persistent(false);         // credentials live in our own NVS namespace
    WiFi.mode(WIFI_STA);            // also creates netif + default event loop
    WiFi.setSleep(false);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, nullptr, nullptr);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnected, nullptr, nullptr);
    // WiFiManager's web server + connect attempts need more stack than before.
    xTaskCreatePinnedToCore(wifi_task, "wifi", 8192, nullptr, 3, nullptr, 0);
}

bool wifi_is_connected() {
    return s_eg && (xEventGroupGetBits(s_eg) & BIT_CONNECTED);
}

bool wifi_wait_connected(TickType_t timeout) {
    return xEventGroupWaitBits(s_eg, BIT_CONNECTED, pdFALSE, pdTRUE, timeout) & BIT_CONNECTED;
}
