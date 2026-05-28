#include "sub_mqtt.h"
#include <config/app_event.h>
// #include "app_queues.h"
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
    appEvent.data = (char *)payload;
    // xQueueSend(uiQueue, &appEvent, 0);
}