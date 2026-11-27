#include "services/pi5_api.h"

#include "app_config.h"
#include "core/config.h"

#define MAX_BODY 8192   // 10 alerts are ~2 KB; anything bigger is not our API

bool pi5_api_configured() { return config_pi5_api_host()[0] != '\0'; }

static int request(Pi5Api &api, const char *method, const char *path, String &body) {
    body = "";
    if (!pi5_api_configured()) return HTTPC_ERROR_CONNECTION_REFUSED;

    // HTTPClient keeps the socket open between calls to the same host when
    // setReuse(true) and the server answers keep-alive (begin() on the same
    // URL host reuses the connected WiFiClient).
    api.http.setReuse(true);
    api.http.setConnectTimeout(PI5_API_TIMEOUT_MS);
    api.http.setTimeout(PI5_API_TIMEOUT_MS);
    if (!api.http.begin(api.tcp, config_pi5_api_host(), config_pi5_api_port(), path))
        return HTTPC_ERROR_CONNECTION_REFUSED;

    char auth[96];
    snprintf(auth, sizeof auth, "Bearer %s", config_pi5_api_token());
    api.http.addHeader("Authorization", auth);
    api.http.addHeader("Accept", "application/json");

    const int code = strcmp(method, "POST") == 0 ? api.http.POST("") : api.http.GET();
    if (code == HTTP_CODE_OK) {
        const int size = api.http.getSize();   // -1 = chunked / unknown
        if (size > MAX_BODY) {
            Serial.printf("[pi5api] %s %s: body too large (%d)\n", method, path, size);
            api.http.end();
            api.tcp.stop();   // drop the unread body with the socket
            return HTTPC_ERROR_TOO_LESS_RAM;
        }
        body = api.http.getString();   // handles chunked transfer encoding
        if (body.length() > MAX_BODY) body = "";
    } else {
        Serial.printf("[pi5api] %s %s -> %d %s\n", method, path, code,
                      code < 0 ? HTTPClient::errorToString(code).c_str() : "");
    }
    api.http.end();   // keeps the socket when reuse is on and the server allows it
    return code;
}

int pi5_api_get(Pi5Api &api, const char *path, String &body) { return request(api, "GET", path, body); }
int pi5_api_post(Pi5Api &api, const char *path, String &body) { return request(api, "POST", path, body); }
