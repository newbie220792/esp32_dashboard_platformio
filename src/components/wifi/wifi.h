#pragma once

// #include <WiFi.h>
// #include <PubSubClient.h>
// #include <Arduino.h>

#include <WiFiClient.h>
#include <PubSubClient.h>

#ifdef __cplusplus
extern "C"
{
#endif
    void initialWifi(void);
    void reconnect(void);

#ifdef __cplusplus
} /* extern "C" */
#endif