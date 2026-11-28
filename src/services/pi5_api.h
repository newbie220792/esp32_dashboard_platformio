#pragma once
// Plain-HTTP client for the Pi 5 REST API (tools/alerts_api.py): base URL and
// bearer token from core/config, 5 s timeout, keep-alive between requests.
// One Pi5Api per task (alerts, location, sysinfo); a Pi5Api is not thread-safe.
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

struct Pi5Api {
    WiFiClient tcp;
    HTTPClient http;
    uint16_t   timeout_ms = 0;   // connect + read timeout; 0 = PI5_API_TIMEOUT_MS (5 s)
};

bool pi5_api_configured();   // PI5_API_HOST set in secrets.h

// Returns the HTTP status (200 = `body` holds the response, at most 8 KB) or a
// negative HTTPClient error. On anything but 200 `body` is left empty.
int pi5_api_get(Pi5Api &api, const char *path, String &body);
int pi5_api_post(Pi5Api &api, const char *path, String &body);

// Binary GET into `buf` (Photos). Needs a Content-Length of 1..cap bytes (chunked
// or bigger answers are refused unread) and reads exactly that many. Returns 200
// with `len` set, the HTTP status, a negative HTTPClient error, or
// HTTPC_ERROR_TOO_LESS_RAM (size) / HTTPC_ERROR_READ_TIMEOUT (short body).
int pi5_api_get_raw(Pi5Api &api, const char *path, uint8_t *buf, size_t cap, size_t &len);
