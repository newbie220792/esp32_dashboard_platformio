#include "display.h"

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "app_config.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;

public:
    LGFX() {
        {
            auto cfg = _bus.config();
            cfg.spi_host = TFT_SPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = TFT_SPI_FREQ;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_PIN_SCLK;
            cfg.pin_mosi = TFT_PIN_MOSI;
            cfg.pin_miso = TFT_PIN_MISO;
            cfg.pin_dc = TFT_PIN_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = TFT_PIN_CS;
            cfg.pin_rst = TFT_PIN_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = TFT_PIN_MISO >= 0;
            cfg.invert = TFT_INVERT;
            cfg.rgb_order = TFT_RGB_ORDER;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel.config(cfg);
        }
#if TFT_PIN_BL >= 0
        {
            auto cfg = _light.config();
            cfg.pin_bl = TFT_PIN_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
#endif
        setPanel(&_panel);
    }
};

static LGFX tft;

#define BUF_LINES 24
static uint16_t *s_buf1;
static uint16_t *s_buf2;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // LVGL renders native little-endian RGB565; LovyanGFX swaps for the panel.
    tft.writePixels(reinterpret_cast<lgfx::rgb565_t *>(px_map), w * h);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

static uint32_t tick_cb() { return millis(); }

lv_display_t *display_init() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_tick_set_cb(tick_cb);

    const size_t buf_bytes = SCREEN_W * BUF_LINES * sizeof(uint16_t);
    s_buf1 = (uint16_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = (uint16_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, s_buf1, s_buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}
