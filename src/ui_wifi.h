#pragma once
#include <lvgl.h>
#include "models.h"

// Wi-Fi setup screens (task 3), on their own LVGL screen so the main
// tileview stays hidden until Wi-Fi connects. LVGL task only.
//   Connecting / ConnectingNew : spinner + network
//   FailedSaved  (screen 4)    : reason, 5 s countdown, Retry / Setup now
//   Portal       (screen 5)    : QR + AP name/password + 192.168.4.1
//   FailedNew    (screen 6)    : reason, "Attempt N", portal still open
lv_obj_t *ui_wifi_create();               // returns the screen object
void ui_wifi_apply(const WifiSetupInfo &info);
