#pragma once
// Lightweight event queue towards the LVGL task. Events carry no data: the
// data lives in the state store (core/state.h). An event only says "look
// now", so a dropped event costs at most one UI poll period, never data.
#include <freertos/FreeRTOS.h>
#include <stdint.h>

enum class StateGroup : uint8_t;

enum class EventType : uint8_t {
    StateChanged,   // .group was written (posted by state.cpp)
    NavNext,        // BOOT short press: next tile
    WifiReopen,     // BOOT long press: show the Wi-Fi setup screens again
};

struct AppEvent {
    EventType  type;
    StateGroup group;   // StateChanged only
};

void events_init();
bool events_post(EventType type);           // any task, never blocks
void events_post_state(StateGroup group);   // any task, never blocks
bool events_receive(AppEvent &ev, TickType_t wait);
