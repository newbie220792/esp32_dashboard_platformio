// JC4827W543C (ESP32-S3, NV3041A 480x272) dashboard: Pi 5 monitor (MQTT),
// weather (Open-Meteo) and home switches. Layers: app -> ui -> services -> core
// -> hal; see app/app.cpp for the init order and tasks.
#include <Arduino.h>

#include "app/app.h"

void setup() { app_init(); }

void loop() { vTaskDelete(nullptr); }   // everything runs in the tasks app_init() created
