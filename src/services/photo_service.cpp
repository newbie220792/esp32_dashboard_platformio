#include "services/photo_service.h"

#include <Arduino.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include <new>
#include <string.h>

#include "core/config.h"
#include "core/parsers.h"
#include "core/state.h"
#include "services/pi5_api.h"
#include "services/wifi_service.h"

#define TICK_MS          50
#define LIST_TIMEOUT_MS  5000
#define PHOTO_TIMEOUT_MS 8000
#define FADE_GUARD_MS    400     // the UI fades for ~300 ms: don't touch the old frame before that
#define MAX_ERRORS       3       // in a row; one more = Offline
#define RETRY_MS         15000   // Offline / empty list: try again after this
#define FRAME_BYTES      ((size_t)PHOTO_W * PHOTO_H * 2)

static QueueHandle_t s_cmd;

// Task-owned (photo task only).
static uint8_t *s_jpeg;          // PHOTO_MAX_BYTES download buffer (PSRAM)
static uint16_t *s_frames[2];    // RGB565 frames (PSRAM), published once in the state
static JPEGDEC *s_dec;           // ~20 KB of tables: PSRAM too
static bool s_alloc_tried, s_alloc_ok;
static Pi5Api s_api;
static PhotoList s_list;
static bool s_have_list;

static PhotoState s_ps;          // what we publish (frames, front, index, status, ...)

static bool s_visible, s_hold, s_need_list;
static bool s_req_next, s_req_prev;
static int s_cur = -1;           // index of the shown photo in s_list, -1 = none yet
static char s_cur_id[PHOTO_ID_LEN];
static bool s_back_ready;        // frames[1 - front] holds s_back_index
static int s_back_index, s_target, s_prev_target;
static uint32_t s_last_swap_ms, s_elapsed_ms, s_last_tick_ms, s_retry_at_ms;
static uint8_t s_errors;

// ---------------- state ----------------

static void publish() {
    state_update_photos([](PhotoState &st, void *ctx) {
        const PhotoState &n = *static_cast<PhotoState *>(ctx);
        if (!memcmp(&st, &n, sizeof st)) return false;
        st = n;
        return true;
    }, &s_ps);
}

static void set_status(PhotoStatus s) {
    if (s_ps.status == s) return;
    s_ps.status = s;
    Serial.printf("[photos] status %d\n", (int)s);
}

// ---------------- buffers ----------------

static void *psram(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

static bool ensure_buffers() {
    if (s_alloc_tried) return s_alloc_ok;
    s_alloc_tried = true;
    s_jpeg = static_cast<uint8_t *>(psram(PHOTO_MAX_BYTES));
    s_frames[0] = static_cast<uint16_t *>(psram(FRAME_BYTES));
    s_frames[1] = static_cast<uint16_t *>(psram(FRAME_BYTES));
    void *dec = psram(sizeof(JPEGDEC));
    s_alloc_ok = s_jpeg && s_frames[0] && s_frames[1] && dec;
    if (!s_alloc_ok) {
        heap_caps_free(s_jpeg);
        heap_caps_free(s_frames[0]);
        heap_caps_free(s_frames[1]);
        heap_caps_free(dec);
        s_jpeg = nullptr;
        s_frames[0] = s_frames[1] = nullptr;
        Serial.println("[photos] PSRAM allocation failed");
        return false;
    }
    s_dec = new (dec) JPEGDEC();
    memset(s_frames[0], 0, FRAME_BYTES);
    memset(s_frames[1], 0, FRAME_BYTES);
    s_ps.frames[0] = s_frames[0];
    s_ps.frames[1] = s_frames[1];
    Serial.printf("[photos] buffers: jpeg %u + 2 x %u B in PSRAM, PSRAM free %u\n", (unsigned)PHOTO_MAX_BYTES,
                  (unsigned)FRAME_BYTES, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return true;
}

// ---------------- network ----------------

// Returns false on a request error (the old list, if any, is kept).
static bool load_list() {
    String body;
    s_api.timeout_ms = LIST_TIMEOUT_MS;
    const int code = pi5_api_get(s_api, "/api/photos", body);
    static PhotoList fresh;   // photo task only (~1.3 KB)
    if (code != 200 || !parse_photo_list(body.c_str(), body.length(), fresh)) {
        Serial.printf("[photos] list failed (%d)\n", code);
        return false;
    }
    if (s_have_list && !strcmp(fresh.rev, s_list.rev)) return true;   // unchanged: keep position

    s_list = fresh;
    s_have_list = true;
    s_back_ready = false;   // it was prepared from the old list
    // Keep showing the same photo if it is still in the new list.
    int pos = -1;
    for (int i = 0; i < s_list.count && s_cur >= 0; i++)
        if (!strcmp(s_list.ids[i], s_cur_id)) pos = i;
    s_cur = pos;
    s_target = s_list.count ? (pos + 1) % s_list.count : 0;
    s_ps.count = s_list.count;
    if (pos >= 0) s_ps.index = pos;
    Serial.printf("[photos] list rev %s: %u photos\n", s_list.rev, (unsigned)s_list.count);
    return true;
}

static int jpeg_draw(JPEGDRAW *d) {
    uint16_t *dst = static_cast<uint16_t *>(d->pUser);
    const int w = min(d->iWidthUsed, PHOTO_W - d->x);
    for (int y = 0; y < d->iHeight && w > 0; y++) {
        const int yy = d->y + y;
        if (yy >= PHOTO_H) break;
        memcpy(dst + (size_t)yy * PHOTO_W + d->x, d->pPixels + (size_t)y * d->iWidth, (size_t)w * 2);
    }
    return 1;
}

// Download + decode list[idx] into the hidden frame.
static bool prepare(int idx) {
    char path[48];
    snprintf(path, sizeof path, "/api/photos/%s.jpg", s_list.ids[idx]);
    size_t len = 0;
    s_api.timeout_ms = PHOTO_TIMEOUT_MS;
    const uint32_t t0 = millis();
    const int code = pi5_api_get_raw(s_api, path, s_jpeg, PHOTO_MAX_BYTES, len);
    if (code != 200) return false;   // 404 (removed), too big, timeout...

    uint16_t *dst = s_frames[1 - s_ps.front];
    const uint32_t t1 = millis();
    if (!s_dec->openRAM(s_jpeg, (int)len, jpeg_draw)) {
        Serial.printf("[photos] %s: not a JPEG (err %d)\n", s_list.ids[idx], s_dec->getLastError());
        return false;
    }
    bool ok = s_dec->getJPEGType() == JPEG_MODE_BASELINE;
    if (!ok) Serial.printf("[photos] %s: progressive JPEG, the Pi 5 must send baseline\n", s_list.ids[idx]);
    if (ok && (s_dec->getWidth() != PHOTO_W || s_dec->getHeight() != PHOTO_H)) {
        Serial.printf("[photos] %s: %dx%d, expected 480x272\n", s_list.ids[idx], s_dec->getWidth(), s_dec->getHeight());
        memset(dst, 0, FRAME_BYTES);   // smaller: black around it; bigger: clipped in jpeg_draw
    }
    if (ok) {
        s_dec->setUserPointer(dst);
        s_dec->setPixelType(RGB565_LITTLE_ENDIAN);   // = LVGL's RGB565 in memory, what the flush path expects
        ok = s_dec->decode(0, 0, 0) == 1;
        if (!ok) Serial.printf("[photos] %s: decode failed (err %d)\n", s_list.ids[idx], s_dec->getLastError());
    }
    s_dec->close();
    if (ok)
        Serial.printf("[photos] %s: %u B, download %lu ms, decode %lu ms\n", s_list.ids[idx], (unsigned)len,
                      (unsigned long)(t1 - t0), (unsigned long)(millis() - t1));
    return ok;
}

// ---------------- slideshow ----------------

static void go_offline(bool retry_later) {
    set_status(PhotoStatus::Offline);
    if (retry_later) s_retry_at_ms = millis() + RETRY_MS;
    s_errors = 0;
}

static void note_error() {
    if (++s_errors > MAX_ERRORS) {
        Serial.println("[photos] too many errors, offline for 15 s");
        go_offline(true);
    }
}

static void show_back() {
    const int prev = s_cur;
    s_ps.front = 1 - s_ps.front;
    s_cur = s_back_index;
    strcpy(s_cur_id, s_list.ids[s_cur]);
    s_ps.index = s_cur;
    s_ps.count = s_list.count;
    s_ps.have_frame = true;
    s_ps.frame_seq++;
    set_status(PhotoStatus::Showing);
    s_back_ready = false;
    s_last_swap_ms = millis();
    s_elapsed_ms = 0;
    s_target = (s_cur + 1) % s_list.count;
    // Back at the start of the list: ask the Pi 5 whether it changed.
    if (s_cur == 0 && prev > 0) s_need_list = true;
}

// Prepare `idx` into the hidden frame. Errors skip forward (`step` +1) or
// backward (-1) for the next try.
static bool try_prepare(int &idx, int step) {
    publish();   // e.g. Loading, before a download that can take seconds
    if (prepare(idx)) {
        s_errors = 0;
        s_back_ready = true;
        s_back_index = idx;
        return true;
    }
    idx = (idx + step + s_list.count) % s_list.count;
    note_error();
    return false;
}

static void step_slideshow() {
    const uint32_t now = millis();
    const uint32_t dt = now - s_last_tick_ms;
    s_last_tick_ms = now;

    if (!ensure_buffers()) {
        set_status(PhotoStatus::NoMemory);
        return;
    }
    if (!pi5_api_configured()) {
        set_status(PhotoStatus::NotConfigured);
        return;
    }
    if (!wifi_service_is_connected()) {   // keep the photo on screen, try again when Wi-Fi is back
        go_offline(false);
        return;
    }
    if ((int32_t)(now - s_retry_at_ms) < 0) return;   // Offline / empty: waiting for the retry time

    if (s_need_list || !s_have_list) {
        s_need_list = false;
        if (!load_list()) {
            if (!s_have_list) go_offline(true);
            else note_error();   // keep playing the old list
            return;
        }
    }
    if (s_list.count == 0) {
        set_status(PhotoStatus::Empty);
        s_ps.count = 0;
        s_retry_at_ms = now + RETRY_MS;   // ask again later: photos may appear
        s_need_list = true;
        return;
    }
    if (s_ps.status == PhotoStatus::Offline || s_ps.status == PhotoStatus::Empty)
        set_status(s_ps.have_frame ? PhotoStatus::Showing : PhotoStatus::Loading);

    // First photo (or the shown one vanished from a new list): show list[s_target] asap.
    if (s_cur < 0) {
        if (!s_ps.have_frame) set_status(PhotoStatus::Loading);
        if (!s_back_ready) try_prepare(s_target, +1);
        if (s_back_ready) show_back();
        return;
    }
    const bool guard_ok = now - s_last_swap_ms >= FADE_GUARD_MS;

    if (s_req_prev && guard_ok) {
        if (s_prev_target < 0) s_prev_target = (s_cur - 1 + s_list.count) % s_list.count;
        s_back_ready = false;   // the prefetched "next" is overwritten
        if (try_prepare(s_prev_target, -1)) {
            s_req_prev = false;
            s_prev_target = -1;
            show_back();
        }
        return;
    }
    if (!s_back_ready && guard_ok) {   // prefetch the next photo while this one is shown
        try_prepare(s_target, +1);
        return;
    }

    const bool counting = !s_ps.paused && !s_hold;
    if (counting) s_elapsed_ms += dt;
    const bool due = s_req_next || (counting && s_elapsed_ms >= s_ps.dwell_s * 1000UL);
    if (due && s_back_ready) {
        s_req_next = false;
        show_back();
    }
}

static void handle(PhotoCmd c) {
    switch (c) {
    case PhotoCmd::Show:
        s_visible = true;
        s_need_list = true;   // compare rev on every visit
        s_retry_at_ms = millis();
        s_last_tick_ms = millis();
        break;
    case PhotoCmd::Hide: s_visible = false; break;
    case PhotoCmd::Next: s_req_next = true; s_req_prev = false; break;
    case PhotoCmd::Prev: s_req_prev = true; s_req_next = false; s_prev_target = -1; break;
    case PhotoCmd::TogglePause:
        s_ps.paused = !s_ps.paused;
        s_elapsed_ms = 0;
        break;
    case PhotoCmd::ToggleDwell:
        s_ps.dwell_s = s_ps.dwell_s == 5 ? 10 : 5;
        config_save_photo_s(s_ps.dwell_s);
        break;
    case PhotoCmd::Hold: s_hold = true; break;
    case PhotoCmd::Release: s_hold = false; break;
    }
}

// ---------------- public ----------------

void photo_service_init() {
    s_cmd = xQueueCreate(8, sizeof(PhotoCmd));
    s_ps.status = PhotoStatus::Idle;
    s_ps.dwell_s = config_load_photo_s();
    s_prev_target = -1;
    publish();
}

void photo_service_task(void *) {
    for (;;) {
        PhotoCmd c;
        if (xQueueReceive(s_cmd, &c, s_visible ? pdMS_TO_TICKS(TICK_MS) : portMAX_DELAY) == pdTRUE) {
            handle(c);
            while (xQueueReceive(s_cmd, &c, 0) == pdTRUE) handle(c);
        }
        if (s_visible) step_slideshow();
        publish();
    }
}

bool photo_service_command(PhotoCmd cmd) { return s_cmd && xQueueSend(s_cmd, &cmd, 0) == pdTRUE; }
