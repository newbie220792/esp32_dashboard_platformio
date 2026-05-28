enum class MessageEventType
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    MQTT_MESSAGE,
    PI_CPU,
    PI_MEM,
    PI_TEMP,
};

struct MessageEvent
{
    MessageEventType type;
    char topic[64];
    char payload[64];
};