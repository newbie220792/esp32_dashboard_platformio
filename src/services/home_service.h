#pragma once
// Home switches: home/config and home/<room>/<panel>/<id>/state -> state store
// (StateGroup::Home); switch commands -> home/<room>/<panel>/<id>/set.
//
// A tap only marks the switch "pending" and publishes .../set. The switch
// changes ON/OFF only when its .../state arrives; after SWITCH_PENDING_MS
// without one, or when MQTT drops, the pending mark is cleared.
#include <stdint.h>

void home_service_init();            // registers the MQTT routes; call before mqtt starts
void home_service_task(void *arg);   // pending timeouts; created by app.cpp

// From the LVGL task. False if MQTT is down, the switch is unknown or already pending.
bool home_service_set(const char *room_id, const char *panel_id, uint16_t switch_id, bool on);
