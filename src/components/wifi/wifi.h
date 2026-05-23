#pragma once

// #include <WiFi.h>
// #include <PubSubClient.h>
// #include <Arduino.h>

#ifdef __cplusplus
extern "C"
{
#endif
    void initialWifi(void);
    void reconnect(void);

#ifdef __cplusplus
} /* extern "C" */
#endif