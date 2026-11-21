#pragma once
#include <freertos/FreeRTOS.h>

// Starts Wi-Fi STA + a supervisor task, configures NTP/timezone.
// (Named wifi_manager to avoid clashing with <WiFi.h> on case-insensitive FS.)
void wifi_start();
bool wifi_is_connected();
// Block the calling task until an IP is obtained (or timeout). Returns state.
bool wifi_wait_connected(TickType_t timeout);
