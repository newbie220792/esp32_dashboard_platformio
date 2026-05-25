/**
 * @file wifi.cpp
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "sub_mqtt.h"
#include "wifi.h"
#include <Arduino.h>
#include <WiFi.h>
// #include <WiFiClient.h>
#include <PubSubClient.h>

const char *ssid = "";
const char *password = "";
const char *mqtt_server = "192.168.1.210";
const char *topic = "pi/monitoring";

WiFiClient espClient;
PubSubClient client(espClient);

void initialWifi(void)
{
    WiFi.begin(ssid, password);
    Serial.println("Wifi connecting....");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Connecting MQTT...");

        String clientId = "ESP32-";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str()))
        {
            Serial.println("connected");

            client.subscribe(topic);
            Serial.println("Subscribed topic");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retry in 2s");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    client.loop();
}