#include "services/weather_service.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "app_config.h"
#include "core/state.h"
#include "services/ca_certs.h"
#include "services/time_service.h"
#include "services/wifi_service.h"

// Open-Meteo answers HTTP/1.1 requests with "Transfer-Encoding: chunked", and
// HTTPClient::getStream() hands out the raw socket, so the parser used to see
// "142\r\n{...". ArduinoJson read the chunk size as a complete number, the
// "current" lookup failed, and fetch() gave up without a word. useHTTP10(true)
// gets a plain body (Connection: close) instead.

// Coordinates come from the Pi 5 (location_service, task 8).
static String build_url(const Location &loc) {
    char url[400];
    snprintf(url, sizeof url,
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
             "&hourly=temperature_2m,weather_code,is_day,precipitation_probability"
             "&forecast_hours=6&wind_speed_unit=kmh&timeformat=unixtime&timezone=Asia%%2FHo_Chi_Minh",
             (double)loc.lat, (double)loc.lon);
    return String(url);
}

// vTaskDelay that ends early when the location changes (reload the weather).
static void wait_or_moved(uint32_t ms, uint32_t &loc_ver) {
    const uint32_t start = millis();
    while (millis() - start < ms) {
        if (state_version(StateGroup::Location) != loc_ver) {
            Serial.println("[weather] location changed, reloading");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static WeatherErr http_error(int code) {
    if (code == HTTPC_ERROR_READ_TIMEOUT) return WeatherErr::Timeout;
    if (code < 0) return WeatherErr::NoNetwork;   // DNS, TCP or TLS failure
    return WeatherErr::ServerError;
}

static bool fetch(const Location &loc, WeatherData &out, WeatherErr &err) {
    const String url = build_url(loc);
    Serial.printf("[weather] GET %s\n", url.c_str());
    Serial.printf("[weather] ip=%s dns=%s heap=%u internal_largest=%u\n", WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str(), (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    WiFiClientSecure tls;
    tls.setCACert(ISRG_ROOT_X1_PEM);
    tls.setHandshakeTimeout(15);   // seconds
    HTTPClient http;
    http.useHTTP10(true);          // no chunked encoding, see top of file
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    if (!http.begin(tls, url)) {
        Serial.println("[weather] http.begin failed");
        err = WeatherErr::NoNetwork;
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[weather] HTTP %d (%s)\n", code, code < 0 ? http.errorToString(code).c_str() : "server");
        http.end();
        err = http_error(code);
        return false;
    }

    // Keep only the fields we read: smaller document, and a renamed field
    // shows up as a missing value instead of a silent wrong one.
    JsonDocument filter;
    filter["utc_offset_seconds"] = true;
    for (const char *k : {"time", "temperature_2m", "relative_humidity_2m", "apparent_temperature", "is_day",
                          "weather_code", "wind_speed_10m"})
        filter["current"][k] = true;
    for (const char *k : {"time", "temperature_2m", "weather_code", "is_day", "precipitation_probability"})
        filter["hourly"][k] = true;

    JsonDocument doc;
    const int size = http.getSize();   // -1 when the server sends no Content-Length
    DeserializationError jerr = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    Serial.printf("[weather] HTTP 200, size=%d, json=%s\n", size, jerr.c_str());
    if (jerr) {
        err = WeatherErr::BadData;
        return false;
    }

    JsonObject cur = doc["current"];
    if (cur.isNull() || !doc["hourly"]["time"].is<JsonArray>()) {
        Serial.println("[weather] response has no current/hourly block");
        err = WeatherErr::BadData;
        return false;
    }
    const long utc_off = doc["utc_offset_seconds"] | 0;
    const long now_t = cur["time"] | 0L;

    out = WeatherData{};
    out.temp = cur["temperature_2m"] | 0.0f;
    out.feels_like = cur["apparent_temperature"] | 0.0f;
    out.humidity = cur["relative_humidity_2m"] | 0.0f;
    out.wind_kmh = cur["wind_speed_10m"] | 0.0f;
    out.code = cur["weather_code"] | 0;
    out.is_day = (cur["is_day"] | 1) != 0;
    out.rain_chance = -1;

    JsonArray times = doc["hourly"]["time"];
    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray codes = doc["hourly"]["weather_code"];
    JsonArray days = doc["hourly"]["is_day"];
    JsonArray rain = doc["hourly"]["precipitation_probability"];
    for (size_t i = 0; i < times.size() && out.hour_count < WEATHER_HOURS; i++) {
        long t = times[i] | 0L;
        // "Rain chance" = the hour in progress (the last one starting at or before now);
        // if the forecast starts after now, the first hour is the best we have.
        if (t <= now_t || out.rain_chance < 0) out.rain_chance = rain[i] | -1;
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

static void post_error(WeatherErr e) { state_set_weather_error(e); }

void weather_service_task(void *) {
    uint32_t backoff_ms = WEATHER_RETRY_MS;
    for (;;) {
        if (!wifi_service_is_connected()) {
            post_error(WeatherErr::NoNetwork);
            wifi_service_wait_connected(portMAX_DELAY);
        }
        // Certificate dates are checked: wait (up to 30 s) for NTP before the TLS handshake.
        for (int i = 0; i < 60 && !time_service_synced(); i++) vTaskDelay(pdMS_TO_TICKS(500));

        uint32_t loc_ver = state_version(StateGroup::Location);
        const Location loc = state_get_location().loc;

        static WeatherData data;   // weather task only
        WeatherErr err = WeatherErr::NoNetwork;
        bool ok = false;
        if (!time_service_synced()) Serial.println("[weather] clock not synced (NTP), TLS would fail");
        else ok = fetch(loc, data, err);

        if (ok) {
            state_set_weather(data);
            Serial.printf("[weather] %s: %.1fC code=%d rain=%d%%, %d hours\n", loc.city, data.temp, data.code,
                          data.rain_chance, data.hour_count);
            backoff_ms = WEATHER_RETRY_MS;
            wait_or_moved(WEATHER_REFRESH_MS, loc_ver);
        } else {
            post_error(err);
            Serial.printf("[weather] failed (err=%d), retry in %lus\n", (int)err, (unsigned long)(backoff_ms / 1000));
            wait_or_moved(backoff_ms, loc_ver);
            backoff_ms = min<uint32_t>(backoff_ms * 2, WEATHER_RETRY_MAX_MS);
        }
    }
}

void weather_service_init() {}
