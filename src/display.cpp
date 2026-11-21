#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

#include "app_config.h"

// NV3041A on a 4-bit QSPI bus. The panel has no reset line; `TFT_IPS` turns on
// colour inversion. Arduino_GFX sends pixels MSB first, so LVGL's native
// little-endian RGB565 buffer is passed as-is (the bus does the byte swap).
static Arduino_DataBus *s_bus =
    new Arduino_ESP32QSPI(TFT_PIN_CS, TFT_PIN_SCK, TFT_PIN_D0, TFT_PIN_D1, TFT_PIN_D2, TFT_PIN_D3);
static Arduino_GFX *s_gfx = new Arduino_NV3041A(s_bus, GFX_NOT_DEFINED, TFT_ROTATION, TFT_IPS);

// Two quarter-screen draw buffers in PSRAM (internal RAM is kept for Wi-Fi/TLS).
#define BUF_LINES (SCREEN_H / 4)

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);
    s_gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(px_map), w, h);
    lv_display_flush_ready(disp);
}

static uint32_t tick_cb() { return millis(); }

lv_display_t *display_init() {
    if (!s_gfx->begin()) Serial.println("[display] NV3041A init failed");
    s_gfx->fillScreen(0x0000);
#if TFT_PIN_BL >= 0
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);   // fixed full brightness
#endif

    lv_init();
    lv_tick_set_cb(tick_cb);

    size_t buf_bytes = SCREEN_W * BUF_LINES * sizeof(uint16_t);
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        // No PSRAM (wrong memory_type?): one small internal buffer instead.
        heap_caps_free(buf1);
        heap_caps_free(buf2);
        buf_bytes = SCREEN_W * 20 * sizeof(uint16_t);
        buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        buf2 = nullptr;
        Serial.println("[display] no PSRAM for draw buffers, using internal RAM");
    }

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}
