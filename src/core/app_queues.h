#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t mqttQueue;
extern QueueHandle_t uiQueue;
// extern QueueHandle_t systemQueue;