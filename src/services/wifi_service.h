#pragma once
// Wi-Fi: boot-time setup flow (secrets.h -> NVS -> WiFiManager portal, task 3),
// then reconnect supervision. Writes StateGroup::Net and StateGroup::WifiSetup.
// (Not named wifi.h: that would shadow <WiFi.h> on case-insensitive file systems.)
#include <freertos/FreeRTOS.h>

enum class WifiCmd : unsigned char { Retry, SetupNow, ResetAndPortal };

void wifi_service_init();            // radio + event handlers; before wifi_service_task
void wifi_service_task(void *arg);   // created by app.cpp

// From the LVGL task (Retry / Setup now buttons) or the BOOT long-press. Never blocks.
bool wifi_service_command(WifiCmd cmd);

bool wifi_service_is_connected();                    // any task
bool wifi_service_wait_connected(TickType_t timeout);   // block until there is an IP
