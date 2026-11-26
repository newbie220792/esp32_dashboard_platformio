#pragma once
// Open-Meteo current conditions + next hours over HTTPS -> state store
// (StateGroup::Weather). Refresh every WEATHER_REFRESH_MS; on failure keep the
// old data, record why, and back off 30 s -> 10 min.
void weather_service_init();
void weather_service_task(void *arg);   // created by app.cpp (TLS needs a ~12 KB stack)
