#include "sub_mqtt.h"

// IP Raspberry Pi chạy Mosquitto

/**payload = {
     * cpu: 10,
     * mem: 10,
     * temp: 30
}*/

// topic_type_t getTopicType(const char *topic)
// {

//     if (strcmp(topic, "home/temp") == 0)
//     {
//         return TOPIC_TEMP;
//     }

//     if (strcmp(topic, "home/humidity") == 0)
//     {
//         return TOPIC_HUMI;
//     }

//     if (strcmp(topic, "home/status") == 0)
//     {
//         return TOPIC_STATUS;
//     }

//     return TOPIC_UNKNOWN;
// }

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Topic: ");
    Serial.println(topic);

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    UIMessage msg;

    // lấy value
    msg.cpu = doc["cpu"] | 0;
    msg.mem = doc["mem"] | 0;
    msg.temp = doc["temp"] | 0.0;

    xQueueSend(uiQueue, &msg, 0);
}