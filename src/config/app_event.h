enum class AppEventType
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    MQTT_MESSAGE,
    PI_CPU,
    PI_MEM,
    PI_TEMP,
    UNKNOW,
};

struct AppEvent
{
    AppEventType type;
    char *data;
};