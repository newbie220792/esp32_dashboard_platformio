#pragma once
// Boot-time Wi-Fi flow (task 3). Runs on the wifi task; every state change is
// posted to the UI as an AppEventType::WifiSetup event.
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
#include <stddef.h>

enum class WifiCmd : unsigned char { Retry, SetupNow, ResetAndPortal };

void wifi_setup_init();

// Blocks the calling (wifi) task until a network is connected. With
// `force_portal`, skips straight to the portal. Writes the credentials that
// worked into ssid/pass (for later reconnects).
void wifi_setup_run(bool force_portal, char *ssid, size_t ssid_len, char *pass, size_t pass_len);

// From the LVGL task (buttons, BOOT long-press). Never blocks.
bool wifi_setup_command(WifiCmd cmd);
// Non-blocking poll for the wifi task outside the setup flow.
bool wifi_setup_take_command(WifiCmd &cmd);

// Called by the STA_DISCONNECTED event handler (any task).
void wifi_setup_note_disconnect(unsigned char reason);

// Forget the network saved through the portal.
void wifi_setup_erase_saved();
