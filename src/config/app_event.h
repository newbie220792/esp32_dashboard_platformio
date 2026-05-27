enum class AppEventType
{
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    PI_CPU,
    PI_MEM,
    PI_TEMP,
    SYS_TIME,
    UNKNOW,
};

struct AppEvent
{
    AppEventType type;
    char *data;
};