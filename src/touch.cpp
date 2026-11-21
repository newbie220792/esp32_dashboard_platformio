#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "app_config.h"

#if TOUCH_ENABLED

#define TOUCH_MODULES_GT911   // selects the driver inside TouchLib.h
#include <TouchLib.h>

static TouchLib s_touch(Wire, TOUCH_PIN_SDA, TOUCH_PIN_SCL, TOUCH_I2C_ADDR);
static int32_t s_last_x, s_last_y;

static void map_point(int32_t rx, int32_t ry, int32_t &x, int32_t &y) {
#if TOUCH_SWAP_XY
    int32_t t = rx; rx = ry; ry = t;
    const int32_t rw = TOUCH_RAW_H, rh = TOUCH_RAW_W;
#else
    const int32_t rw = TOUCH_RAW_W, rh = TOUCH_RAW_H;
#endif
    x = rx * SCREEN_W / rw;
    y = ry * SCREEN_H / rh;
#if TOUCH_INVERT_X
    x = SCREEN_W - 1 - x;
#endif
#if TOUCH_INVERT_Y
    y = SCREEN_H - 1 - y;
#endif
    x = LV_CLAMP(0, x, SCREEN_W - 1);
    y = LV_CLAMP(0, y, SCREEN_H - 1);
}

static void read_cb(lv_indev_t *, lv_indev_data_t *data) {
    if (s_touch.read()) {
        TP_Point p = s_touch.getPoint(0);
        map_point(p.x, p.y, s_last_x, s_last_y);
#if TOUCH_DEBUG
        Serial.printf("[touch] raw=%d,%d -> %ld,%ld\n", p.x, p.y, (long)s_last_x, (long)s_last_y);
#endif
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = s_last_x;
    data->point.y = s_last_y;
}

lv_indev_t *touch_init() {
#if TOUCH_PIN_RST >= 0
    pinMode(TOUCH_PIN_RST, OUTPUT);
    digitalWrite(TOUCH_PIN_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_PIN_RST, HIGH);
    delay(100);
#endif
    Wire.begin(TOUCH_PIN_SDA, TOUCH_PIN_SCL);
    s_touch.init();

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_cb);
    return indev;
}

#else
lv_indev_t *touch_init() { return nullptr; }
#endif
