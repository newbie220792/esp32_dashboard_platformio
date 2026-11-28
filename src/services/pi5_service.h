#pragma once
// Raspberry Pi 5 monitor data: pi5/stats and pi5/services -> state store
// (StateGroup::Pi5Stats, StateGroup::Services). Handlers run on the mqtt task.
void pi5_service_init();   // registers the MQTT routes; call before mqtt starts
