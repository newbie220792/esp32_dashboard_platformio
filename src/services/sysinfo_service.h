#pragma once
// System info screen data (task 9). Never calls LVGL.
//
//  * sysinfo_service_read(): assembles a SysInfo snapshot from the state store
//    (Wi-Fi, MQTT, Pi 5 health), the SNTP sync time and hal/sysinfo (heap,
//    PSRAM, uptime, flash). Cheap: the System info screen calls it every second.
//  * Its task measures the sketch size once (slow, hashes the image), then,
//    only while the screen is open and Wi-Fi has an IP, sends GET /api/health
//    to the Pi 5 every SYSINFO_API_MS (3 s timeout, one request at a time) and
//    writes the outcome + round trip to StateGroup::ApiHealth. If the Pi 5 has
//    no /api/health (404) it times GET /api/alerts/summary instead.
#include "core/models.h"

void sysinfo_service_init();
void sysinfo_service_task(void *arg);   // created by app.cpp

void sysinfo_service_set_open(bool open);   // System info screen shown / left; never blocks
void sysinfo_service_read(SysInfo &out);    // any task
