#include "services/mqtt/sub_mqtt.h"
#include "wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <config/ui_constants.h>

WiFiClient espClient;
PubSubClient client(espClient);

void initialWifi(void)
{
    WiFi.begin(UI::SSID, UI::PASSWORD);
    Serial.println("Wifi connecting....");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.println(WiFi.localIP());

    client.setServer(UI::MQTT_SERVER_IP, UI::MQTT_SERVER_PORT);
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

            client.subscribe(UI::PI_MONITORING_TOPIC);
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