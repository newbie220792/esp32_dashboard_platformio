#pragma once
// Pi 5 location (task 8): GET /api/location -> StateGroup::Location (+ NVS copy).
// The city is shown read-only in Settings and on the Weather tab; the weather
// service fetches for its lat/lon. Before the first answer: the NVS copy, else
// DEFAULT_CITY / DEFAULT_LAT / DEFAULT_LON (app_config.h).
void location_service_init();          // NVS copy or default into the state store
void location_service_task(void *arg); // at boot, after every reconnect, every LOCATION_REFRESH_MS
