
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void callback(char* topic, byte* payload, unsigned int length);

#ifdef __cplusplus
} /* extern "C" */
#endif