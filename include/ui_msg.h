#pragma once

// #include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef struct
{
    int cpu;
    int mem;
    float temp;

} UIMessage;

extern QueueHandle_t uiQueue;