#pragma once
// Network tasks -> LVGL task. Network code never calls LVGL directly; it posts
// a copy of its data here and the LVGL task applies it (see ui_process_events).

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "models.h"

enum class AppEventType : uint8_t {
    WifiState,     // .flag = connected
    MqttState,     // .flag = connected
    PiStats,       // .pi
    Services,      // .services   (heap; receiver takes ownership, delete)
    HomeConfig,    // .home       (heap; receiver takes ownership, delete)
    SwitchState,   // .sw
    Weather,       // .weather
    WeatherError,
};

struct AppEvent {
    AppEventType type;
    union {
        bool         flag;
        PiStats      pi;
        ServiceList *services;
        HomeConfig  *home;
        SwitchState  sw;
        WeatherData  weather;
    };
};

void app_events_init();
// Safe from any task. Never blocks; returns false if the queue is full, in
// which case the caller still owns any heap pointer in `ev`.
bool app_events_post(const AppEvent &ev);
bool app_events_receive(AppEvent &ev, TickType_t wait);

void app_post_flag(AppEventType type, bool flag);
