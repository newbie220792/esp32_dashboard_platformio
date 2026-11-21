#pragma once
// Network tasks -> LVGL task. Network code never calls LVGL directly; it posts
// a copy of its data here and the LVGL task applies it (see ui_process_events).

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct PiStats {
    float cpu;        // %
    float ram;        // %
    float ram_used;   // GB
    float ram_total;  // GB
    float temp;       // deg C
};

#define WEATHER_HOURS 4

struct WeatherHour {
    uint8_t hour;     // local hour 0..23
    int16_t code;     // WMO weather code
    bool    is_day;
    float   temp;
};

struct WeatherData {
    float   temp;
    float   feels_like;
    float   humidity;
    float   wind_kmh;
    int16_t code;
    bool    is_day;
    WeatherHour hours[WEATHER_HOURS];
    uint8_t hour_count;
};

enum class AppEventType : uint8_t {
    WifiState,     // .flag = connected
    MqttState,     // .flag = connected
    PiStats,       // .pi
    Weather,       // .weather
    WeatherError,
};

struct AppEvent {
    AppEventType type;
    union {
        bool        flag;
        PiStats     pi;
        WeatherData weather;
    };
};

void app_events_init();
bool app_events_post(const AppEvent &ev);           // safe from any task
bool app_events_receive(AppEvent &ev, TickType_t wait);

void app_post_flag(AppEventType type, bool flag);
