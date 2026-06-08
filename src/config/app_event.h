enum class AppEventType
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    MQTT_MESSAGE,
    PI_CPU,
    PI_MEM,
    PI_TEMP,
    PI_DISK,
    WEATHER,
    TEMPERATURE,
    HUMIDITY,
    RAIN,
    IS_DAY,
    UNKNOW,
};

struct AppEvent
{
    AppEventType type;
    char data[64];
};