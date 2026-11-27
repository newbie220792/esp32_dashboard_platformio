#pragma once
// Pi 5 alerts over REST (task 8) -> StateGroup::Alerts. Runs in its own task;
// one request at a time, never from the LVGL task.
//
//  * Summary (bell badge): every ALERTS_SUMMARY_MS while Wi-Fi is up; on error
//    back off 30 s, 60 s, ... 5 min. Offline: the last value stays.
//  * List (at most ALERTS_MAX): fetched when the Alerts screen opens, then
//    every ALERTS_LIST_MS or when the summary's latest_id moves, while open.
//  * Clear all: POST /api/alerts/ack-all; only a 200 empties the list.
//
// The commands below only set flags for the task: they never block.
void alerts_service_init();
void alerts_service_task(void *arg);

void alerts_service_set_open(bool open);   // Alerts screen shown / left
void alerts_service_refresh();             // "Retry"
void alerts_service_clear_all();           // "Clear all"
