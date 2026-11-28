#pragma once
// Photos tab (task 10): slideshow from the Pi 5 REST API.
//
//   GET /api/photos          list + rev (the Pi 5 decides which photos, in which order)
//   GET /api/photos/<id>.jpg 480x272 baseline JPEG, Content-Length <= 96 KB
//
// Downloads and JPEG decoding (JPEGDEC -> RGB565) run on this service's task,
// into two PSRAM frames: one shown, one being prepared. The result goes to
// StateGroup::Photos; the UI only swaps which frame is on top. No LVGL here.
//
// Works only while the tab is visible (PhotoCmd::Show / Hide): leaving the tab
// stops downloads and the slide timer, coming back continues from the photo
// on screen. Wi-Fi down or more than 3 errors in a row: status Offline, the
// last photo stays, retry after 15 s.
#include <stdint.h>

enum class PhotoCmd : uint8_t {
    Show,          // tab became visible
    Hide,          // tab left (other tab, overlay, Wi-Fi screen)
    Next,          // next photo now
    Prev,          // previous photo (downloaded again)
    TogglePause,   // Pause <-> Play
    ToggleDwell,   // 5 s <-> 10 s, saved as NVS photo_s
    Hold,          // controls shown: the slide timer stops counting
    Release,       // controls hidden: it counts again
};

void photo_service_init();            // NVS photo_s -> state; buffers are allocated on first Show
void photo_service_task(void *arg);   // created by app.cpp (HTTP + decode: ~8 KB stack)
bool photo_service_command(PhotoCmd cmd);   // LVGL task; never blocks
