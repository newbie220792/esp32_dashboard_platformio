#include "sub_mqtt.h"
#include <config/app_event.h>
#include "core/app_queues.h"
#include <config/topic.h>

AppEventType getAppEventType(const char *topic)
{
    if (strcmp(topic, Topics::Subscribe::PI_CPU) == 0)
    {
        return AppEventType::PI_CPU;
    }
    if (strcmp(topic, Topics::Subscribe::PI_MEM) == 0)
    {
        return AppEventType::PI_MEM;
    }
    if (strcmp(topic, Topics::Subscribe::PI_TEMP) == 0)
    {
        return AppEventType::PI_TEMP;
    }
    if (strcmp(topic, Topics::Subscribe::PI_DISK) == 0)
    {
        return AppEventType::PI_DISK;
    }
    if (strcmp(topic, Topics::Subscribe::TEMPERATURE) == 0)
    {
        return AppEventType::TEMPERATURE;
    }
    if (strcmp(topic, Topics::Subscribe::WEATHER) == 0)
    {
        return AppEventType::WEATHER;
    }
    if (strcmp(topic, Topics::Subscribe::HUMIDITY) == 0)
    {
        return AppEventType::HUMIDITY;
    }
    if (strcmp(topic, Topics::Subscribe::RAIN) == 0)
    {
        return AppEventType::RAIN;
    }
    if (strcmp(topic, Topics::Subscribe::IS_DAY) == 0)
    {
        return AppEventType::IS_DAY;
    }
    return AppEventType::UNKNOW;
}

void callback(char *topic, byte *payload, unsigned int length)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
    }

    AppEvent appEvent;

    appEvent.type = getAppEventType(topic);

    strcpy(
        appEvent.data,
        (char *)payload);

    xQueueSend(uiQueue, &appEvent, 0);
}