enum class MessageEventType
{
    MQTT_MESSAGE,
    WIFI_DISCONNECTED,
};

struct MessageEvent
{
    MessageEventType type;
    const char *topic;
    const char *payload;
};