# Task 7: Tái cấu trúc project theo chức năng và phân tầng service

Mục tiêu: chia code thành các tầng rõ ràng, mỗi chức năng một module, để thêm tính năng (như tab Claude usage ở `task_6.md`) mà không phải sửa lan nhiều file. **Không đổi hành vi**: sau khi tái cấu trúc, mọi chức năng cũ chạy y như trước.

> File này viết khi chưa xem code hiện tại. Bước 1 là đọc code thật và lập bảng ánh xạ file cũ → file mới trước khi di chuyển.

## 1. Các tầng và quy tắc phụ thuộc

```
app        (main, khởi tạo, tạo task)
  │
ui         (màn hình, widget)          ← chỉ ĐỌC state, GỬI lệnh qua service
  │
services   (wifi, mqtt, weather, ota, ...)   ← KHÔNG gọi LVGL
  │
core       (state store, event queue, config, log)
  │
hal        (display, touch, backlight, chân GPIO)
```

Quy tắc bắt buộc:

1. Chỉ được phụ thuộc **xuống dưới**. `ui` không include header của `hal`; `services` không include `ui`; `core` không include `services`.
2. **Chỉ tầng `ui` được gọi LVGL**, và chỉ từ task LVGL. Service không bao giờ gọi `lv_*`.
3. Service giao tiếp với UI qua **state store** (dữ liệu hiện tại) và **event queue** (sự kiện). UI đọc state bằng `lv_timer`; UI gửi lệnh bằng hàm của service (ví dụ `home_service_set(room, panel, idx, on)`).
4. Mỗi service có đúng một cặp `.h/.cpp`, một hàm `xxx_init()`, một task riêng nếu cần chạy nền, và không dùng biến toàn cục ngoài state store.
5. Không hard-code Wi-Fi, mật khẩu, broker trong code: chỉ đọc từ `config`.

## 2. Cấu trúc thư mục đề xuất

```
platformio.ini
partitions_ota.csv
CLAUDE.md
include/
  lv_conf.h
  secrets.h              (không commit)
  board_pins.h           (chân GPIO của JC4827W543C, lấy từ tài liệu board)
src/
  main.cpp               (chỉ gọi app_init và vòng lặp)
  app/
    app.cpp/.h           (khởi tạo theo thứ tự, tạo các task)
  hal/
    display.cpp/.h       (NV3041A QSPI, flush LVGL)
    touch.cpp/.h         (GT911, read callback)
    backlight.cpp/.h
  core/
    state.cpp/.h         (struct trạng thái dùng chung + mutex)
    events.cpp/.h        (queue sự kiện: kiểu, payload)
    config.cpp/.h        (secrets.h, NVS/Preferences, giá trị mặc định)
    log.h
  services/
    wifi_service.cpp/.h        (kết nối secrets.h, WiFiManager, trạng thái, thử lại)
    time_service.cpp/.h        (NTP, múi giờ Asia/Ho_Chi_Minh)
    mqtt_service.cpp/.h        (kết nối, subscribe, publish, định tuyến theo topic)
    pi5_service.cpp/.h         (parse pi5/stats, pi5/services → state)
    home_service.cpp/.h        (parse home/config, .../state; gửi .../set)
    claude_usage_service.cpp/.h(parse claude/usage → state)
    weather_service.cpp/.h     (gọi Open-Meteo, parse, thử lại, giữ dữ liệu cũ)
    ota_service.cpp/.h         (ArduinoOTA, kéo từ Pi 5, MD5, tiến trình)
  ui/
    ui_manager.cpp/.h    (tileview, chuyển màn, lv_timer cập nhật, nhận event)
    theme.cpp/.h         (màu, font, kích thước, style dùng chung)
    components/
      status_bar.cpp/.h  (chấm MQTT, chỉ trang, icon Wi-Fi, giờ)
      gauge.cpp/.h       (cung CPU/RAM/TEMP)
      switch_tile.cpp/.h (ô công tắc ON/OFF)
      toast.cpp/.h       (thông báo ngắn)
    screens/
      screen_monitor.cpp/.h   (màn 1: đồng hồ + danh sách service)
      screen_weather.cpp/.h
      screen_home.cpp/.h
      screen_claude.cpp/.h
      screen_wifi.cpp/.h      (3 chế độ: lỗi, portal, thử lại; nút Back)
      screen_ota.cpp/.h       (tiến trình cập nhật)
tools/
  pi5_publisher.py       (publish pi5/stats, pi5/services)
  claude_usage_publisher.py
  ota_server.py          (phát firmware.bin, version.json)
docs/
  mqtt_topics.md         (bảng topic, JSON, retain, QoS)
```

PlatformIO biên dịch đệ quy thư mục `src/`; thêm `build_flags = -I src` để include theo dạng `#include "services/wifi_service.h"`.

## 3. Trách nhiệm từng tầng

**hal**: chỉ nói chuyện với phần cứng. Không biết LVGL widget, không biết Wi-Fi hay MQTT. `display` và `touch` đăng ký driver cho LVGL (cho phép `hal` include LVGL ở mức driver).

**core**

- `state`: struct dạng `net_state` (wifi, mqtt, rssi, ip), `pi5_state` (cpu, ram, temp, danh sách service), `weather_state`, `home_state` (phòng → cụm → công tắc), `claude_state`. Mỗi nhóm có mutex hoặc cơ chế sao chép an toàn (ví dụ hàm `state_get_xxx(copy)` và `state_set_xxx`).
- `events`: queue sự kiện nhẹ (ví dụ `EV_WIFI_CHANGED`, `EV_STATE_UPDATED`, `EV_OTA_PROGRESS`), UI đọc ở task LVGL.
- `config`: nạp `secrets.h`, đọc/ghi NVS; không chứa logic mạng.

**services**: mỗi file một nghiệp vụ, tự chạy trong task riêng (stack ghi rõ trong code), ghi kết quả vào `state` và bắn event. `mqtt_service` chỉ lo kết nối và **định tuyến**: đăng ký handler theo tiền tố topic (`pi5/#` → `pi5_service`, `home/#` → `home_service`, `claude/#` → `claude_usage_service`). Thêm topic mới = thêm một handler, không sửa `mqtt_service`.

**ui**: `ui_manager` tạo tileview và các màn; một `lv_timer` (khoảng 500 ms) đọc `state` rồi cập nhật tất cả widget (kể cả thanh trạng thái ở mọi màn). Component dùng chung (status bar, gauge, switch tile) viết một lần, các màn gọi lại. `theme` giữ toàn bộ màu và font, không rải mã màu trong từng màn.

**app**: thứ tự khởi tạo cố định: `config` → `hal` → LVGL → `ui_manager` → `wifi_service` → `time_service` → `mqtt_service` → các service dữ liệu → `ota_service`.

## 4. Quy ước đặt tên và task

- Tiền tố theo module: `wifi_`, `mqtt_`, `home_`, `ui_`, `hal_`.
- Mỗi service có `xxx_init()` và (nếu cần) `xxx_task()` tạo trong `app.cpp`; ghi kích thước stack và độ ưu tiên ở một chỗ (`app.cpp`).
- Task LVGL là task duy nhất gọi `lv_timer_handler()`.
- Mỗi header chỉ lộ ra hàm cần thiết; chi tiết nội bộ để `static` trong `.cpp`.

## 5. Việc cần làm (theo thứ tự, mỗi bước build được)

1. Đọc code hiện tại, lập bảng ánh xạ file cũ → module mới (đưa vào PR/ghi chú).
2. Tạo khung thư mục và `core` (`state`, `events`, `config`); chưa đổi hành vi.
3. Tách `hal` (display, touch, backlight) và `board_pins.h`.
4. Tách từng service theo thứ tự: `wifi_service` → `time_service` → `mqtt_service` (định tuyến) → `pi5_service` → `weather_service` → `home_service` → `ota_service` → `claude_usage_service`. Sau mỗi service: `pio run` và thử trên board.
5. Tách `ui`: `theme`, `components`, rồi từng `screen_*`; chuyển toàn bộ cập nhật widget vào `lv_timer` đọc `state`; xóa mọi lệnh `lv_*` khỏi service và callback.
6. Rút gọn `main.cpp` chỉ còn gọi `app_init()` và vòng lặp.
7. Viết `docs/mqtt_topics.md`; di chuyển script Pi 5/Mac vào `tools/`.
8. Cập nhật `CLAUDE.md` (cấu trúc mới, quy tắc phụ thuộc, thứ tự khởi tạo) và các `task_*.md` nếu có tên file cũ.

## 6. Tiêu chí hoàn thành

- Không có `#include "ui/..."` hoặc `lv_*` trong `services/` và `core/` (kiểm tra bằng `grep`).
- Không có include ngược tầng (hal không include services, v.v.).
- Mọi chức năng cũ chạy đúng như trước: Wi-Fi/WiFiManager, MQTT, 4 tab, công tắc, thời tiết, OTA, icon Wi-Fi.
- Thêm một topic hoặc một tab mới chỉ cần: 1 service + 1 screen + đăng ký handler/tile, không sửa các module khác.
- `pio run` build thành công, không warning mới; kích thước firmware không tăng đáng kể (phải còn trong vùng OTA ~1.94 MB).

## 7. Cần xác nhận

- Có muốn giữ code ở `src/` (đơn giản) hay chuyển từng module thành thư viện trong `lib/` (mỗi module có `library.json`, tách biệt hơn nhưng nhiều file cấu hình hơn)? Đề xuất: `src/` cho project này.
- Dùng C++ class hay hàm thuần C kiểu `module_function()`? Đề xuất: hàm thuần kèm struct, vì nhẹ và hợp với LVGL.
- Code hiện tại đã tách phần nào chưa (để bước 1 chia công việc đúng)?
