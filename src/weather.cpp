#include "weather.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "app_config.h"
#include "app_events.h"
#include "wifi_manager.h"


static String build_url() {
    char url[384];
    snprintf(url, sizeof url,
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
             "&hourly=temperature_2m,weather_code,is_day"
             "&forecast_hours=6&wind_speed_unit=kmh&timeformat=unixtime&timezone=Asia%%2FHo_Chi_Minh",
             (double)WEATHER_LAT, (double)WEATHER_LON);
    return String(url);
}

static bool fetch(WeatherData &out) {
    WiFiClientSecure tls;
    tls.setInsecure();   // public, read-only data; skip CA bundle to save flash/RAM
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(tls, build_url())) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[weather] HTTP %d\n", code);
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        Serial.printf("[weather] JSON error: %s\n", err.c_str());
        return false;
    }

    JsonObject cur = doc["current"];
    if (cur.isNull()) return false;
    const long utc_off = doc["utc_offset_seconds"] | 0;
    const long now_t = cur["time"] | 0L;

    out = WeatherData{};
    out.temp = cur["temperature_2m"] | 0.0f;
    out.feels_like = cur["apparent_temperature"] | 0.0f;
    out.humidity = cur["relative_humidity_2m"] | 0.0f;
    out.wind_kmh = cur["wind_speed_10m"] | 0.0f;
    out.code = cur["weather_code"] | 0;
    out.is_day = (cur["is_day"] | 1) != 0;

    JsonArray times = doc["hourly"]["time"];
    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray codes = doc["hourly"]["weather_code"];
    JsonArray days = doc["hourly"]["is_day"];
    for (size_t i = 0; i < times.size() && out.hour_count < WEATHER_HOURS; i++) {
        long t = times[i] | 0L;
        if (t <= now_t) continue;   // skip the hour already in progress
        time_t local = (time_t)(t + utc_off);
        struct tm tmv;
        gmtime_r(&local, &tmv);
        WeatherHour &h = out.hours[out.hour_count++];
        h.hour = tmv.tm_hour;
        h.temp = temps[i] | 0.0f;
        h.code = codes[i] | 0;
        h.is_day = (days[i] | 1) != 0;
    }
    return true;
}

static void weather_task(void *) {
    for (;;) {
        wifi_wait_connected(portMAX_DELAY);
        AppEvent ev{};
        if (fetch(ev.weather)) {
            ev.type = AppEventType::Weather;
            app_events_post(ev);
            Serial.printf("[weather] %.1fC code=%d, %d hours\n", ev.weather.temp, ev.weather.code,
                          ev.weather.hour_count);
            vTaskDelay(pdMS_TO_TICKS(WEATHER_REFRESH_MS));
        } else {
            app_post_flag(AppEventType::WeatherError, false);
            vTaskDelay(pdMS_TO_TICKS(WEATHER_RETRY_MS));
        }
    }
}

void weather_start() {
    // TLS handshake needs a fairly deep stack.
    xTaskCreatePinnedToCore(weather_task, "weather", 12288, nullptr, 1, nullptr, 0);
}
