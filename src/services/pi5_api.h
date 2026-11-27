#pragma once
// Plain-HTTP client for the Pi 5 REST API (tools/alerts_api.py): base URL and
// bearer token from core/config, 5 s timeout, keep-alive between requests.
// One Pi5Api per task (alerts, location); a Pi5Api is not thread-safe.
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

struct Pi5Api {
    WiFiClient tcp;
    HTTPClient http;
};

bool pi5_api_configured();   // PI5_API_HOST set in secrets.h

// Returns the HTTP status (200 = `body` holds the response, at most 8 KB) or a
// negative HTTPClient error. On anything but 200 `body` is left empty.
int pi5_api_get(Pi5Api &api, const char *path, String &body);
int pi5_api_post(Pi5Api &api, const char *path, String &body);
