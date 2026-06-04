#include "services/mqtt/sub_mqtt.h"
#include "wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <config/ui_constants.h>
#include <config/topic.h>
#include <config/app_event.h>
#include "core/app_queues.h"

WiFiClient espClient;
PubSubClient client(espClient);

void initTime()
{
    configTime(
        7 * 3600,
        0,
        "time.google.com",
        "pool.ntp.org",
        "time.windows.com");

    struct tm timeinfo;

    while (!getLocalTime(&timeinfo))
    {
        Serial.println("Waiting NTP...");
        delay(500);
    }

    Serial.println("Time synchronized");
}

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

    initTime();
    AppEvent appEvent;
    appEvent.type = AppEventType::WIFI_CONNECTED;

    xQueueSend(uiQueue, &appEvent, 0);

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

            Serial.println("Subscribed topic: ");
            for (size_t i = 0; i < Topics::Subscribe::COUNT; i++)
            {
                client.subscribe(Topics::Subscribe::ALL[i]);
                Serial.print(Topics::Subscribe::ALL[i]);
                Serial.print(", ");
            }
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

void pushMessage(const char *topic, const char *payload)
{
    if (client.connected())
    {
        client.publish(topic, payload);
        Serial.print("Publish message ");
        Serial.print(payload);
        Serial.print(" to ");
        Serial.print(topic);
        Serial.println(" success!!");
    }
    else
    {
        Serial.println("Client was not connectd to mqtt server!!!");
        return;
    }
}