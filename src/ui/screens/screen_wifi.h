#pragma once
#include <lvgl.h>
#include "core/models.h"

// Wi-Fi setup screens (task 3), on their own LVGL screen so the main
// tileview stays hidden until Wi-Fi connects. LVGL task only.
//   Connecting / ConnectingNew : spinner + network
//   FailedSaved  (screen 4)    : reason, 5 s countdown, Retry / Setup now
//   Portal       (screen 5)    : QR + AP name/password + 192.168.4.1
//   FailedNew    (screen 6)    : reason, "Attempt N", portal still open
// `on_back` runs when the top-bar "Back" button is tapped.
lv_obj_t *screen_wifi_create(void (*on_back)());   // returns the screen object
void screen_wifi_apply(const WifiSetupInfo &info);
