#pragma once
class UI
{
public:
    // Screen
    static constexpr int SCREEN_WIDTH = 480;
    static constexpr int SCREEN_HEIGHT = 270;

    // Header
    static constexpr int HEADER_HEIGHT = 30;

    // MQTT
    static constexpr const char *MQTT_SERVER_IP = "192.168.1.210";
    static constexpr int MQTT_SERVER_PORT = 1883;

    // WiFi
    static constexpr const char *SSID = "";
    static constexpr const char *PASSWORD = "";
};