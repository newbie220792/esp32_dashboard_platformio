#include "core/events.h"

#include <freertos/queue.h>

#include "core/state.h"

static QueueHandle_t s_queue = nullptr;

void events_init() { s_queue = xQueueCreate(32, sizeof(AppEvent)); }

static bool post(const AppEvent &ev) {
    // Never block a network task on the UI; the UI's poll timer catches up.
    return s_queue && xQueueSend(s_queue, &ev, 0) == pdTRUE;
}

bool events_post(EventType type) {
    AppEvent ev{};
    ev.type = type;
    return post(ev);
}

void events_post_state(StateGroup group) {
    AppEvent ev{};
    ev.type = EventType::StateChanged;
    ev.group = group;
    post(ev);
}

bool events_receive(AppEvent &ev, TickType_t wait) {
    return s_queue && xQueueReceive(s_queue, &ev, wait) == pdTRUE;
}
