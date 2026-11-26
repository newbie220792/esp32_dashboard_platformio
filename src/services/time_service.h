#pragma once
// NTP + timezone (Asia/Ho_Chi_Minh, TZ_POSIX). Its task waits for Wi-Fi, starts
// SNTP once, and exits; the clock then stays in sync on its own.
void time_service_init();
void time_service_task(void *arg);   // created by app.cpp
bool time_service_synced();          // any task: true once the clock is real
