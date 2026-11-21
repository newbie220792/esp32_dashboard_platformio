#include "app_events.h"

static QueueHandle_t s_queue = nullptr;

void app_events_init() {
    s_queue = xQueueCreate(24, sizeof(AppEvent));
}

bool app_events_post(const AppEvent &ev) {
    if (!s_queue) return false;
    // Never block a network task on the UI; drop if the UI is behind.
    return xQueueSend(s_queue, &ev, 0) == pdTRUE;
}

bool app_events_receive(AppEvent &ev, TickType_t wait) {
    return s_queue && xQueueReceive(s_queue, &ev, wait) == pdTRUE;
}

void app_post_flag(AppEventType type, bool flag) {
    AppEvent ev{};
    ev.type = type;
    ev.flag = flag;
    app_events_post(ev);
}
