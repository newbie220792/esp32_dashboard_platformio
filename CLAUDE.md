# ESP32 LVGL – Pi 5 Monitor, Weather & Home switches

Firmware ESP32 hiển thị giao diện LVGL 9 gồm 3 màn hình vuốt ngang (`lv_tileview`):

1. **Pi 5 Monitor**: CPU %, RAM %, nhiệt độ và danh sách service của Raspberry Pi 5 qua MQTT.
2. **Weather**: thời tiết thời gian thực (Open-Meteo, không cần API key).
3. **Home switches**: điều khiển công tắc nhà theo phòng → cụm → 3 công tắc qua MQTT.

Ngoài dải vuốt có 2 màn phủ (mở bằng chuông và bánh răng trên thanh trạng thái, task 8):
**Alerts** (tối đa 10 cảnh báo từ Pi 5 qua REST) và **Settings** (độ sáng, tắt màn hình, °C/°F,
vị trí chỉ đọc lấy từ Pi 5).

Thiết kế tham chiếu: https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd
Đặc tả công việc: `tasks/`.

## Phần cứng: Guition JC4827W543C (4.3 inch, cảm ứng điện dung)

- MCU: ESP32-S3-WROOM-1 N4R8 (flash 4 MB, PSRAM 8 MB octal). Mô tả của người bán ghi "LX6" là sai.
  `board = esp32-s3-devkitc-1` + `board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM`;
  thiếu `qio_opi` thì không có PSRAM (draw buffer tự lùi về 20 dòng trong RAM nội, chậm hơn).
- Màn hình: IPS 480x272, driver **NV3041A qua QSPI**. Chân trong `include/board_pins.h`
  (lấy từ cấu hình ESPHome của board): CS=45 CLK=47 D0=21 D1=48 D2=40 D3=39, đèn nền BL=1 điều khiển
  bằng PWM LEDC kênh 7, 20 kHz (`hal_backlight_set()`). Không có chân reset màn hình.
- Cảm ứng: GT911 (I2C, TouchLib trong `lib/`) SDA=8 SCL=4 RST=38 INT=3, tọa độ thô 480x272.
  Hiệu chỉnh bằng `TOUCH_DEBUG`, `TOUCH_SWAP_XY`, `TOUCH_INVERT_X/Y` (xoay màn 180° thì đảo cả X và Y).
- Nút BOOT (GPIO0): bấm nhanh để chuyển màn; giữ 5 giây để xóa Wi-Fi đã lưu và mở chế độ cấu hình.
- Chưa xác minh: địa chỉ I2C GT911 (đang dùng 0x5D, dự phòng 0x14), chân thẻ SD và loa. Thẻ SD hiện không dùng.

### Lưu ý với board này

- NV3041A cần **đảo màu** (`TFT_IPS`, tham số `ips` của `Arduino_NV3041A`) và **đổi byte**: bus QSPI của
  GFX Library tự gửi MSB trước, nên buffer RGB565 của LVGL đưa thẳng vào `draw16bitRGBBitmap`. Màu sai thì
  thử đổi hai chỗ này trước.
- Chỉ xoay được 0° hoặc 180° (`TFT_ROTATION` 0 hoặc 2), không dùng 90°/270°.
- Thẻ SD dùng chung bus với màn: nếu sau này dùng SD thì phải sắp thứ tự thao tác và kiểm tra lại cảm ứng.
- Flash chỉ 4 MB (`huge_app.csv`, 3 MB cho app, không có OTA; nạp firmware qua USB): chỉ bật font và widget
  LVGL đang dùng. Sau task 8 firmware ~1.48 MB, vẫn vừa vùng OTA ~1.94 MB nếu sau này chia lại phân vùng.
- Draw buffer: 2 buffer 1/4 màn hình (480x68) trong PSRAM, chế độ partial.

## Công nghệ

- PlatformIO + Arduino framework (`espressif32@6.9.0`, Arduino-ESP32 2.0.17)
- LVGL 9.2, **GFX Library for Arduino 1.4.9** (driver màn hình; bản 1.5+ cần Arduino-ESP32 3.x nên không nâng),
  TouchLib (cảm ứng), WiFiManager (cổng cấu hình Wi-Fi). Không dùng LovyanGFX/TFT_eSPI (không có NV3041A QSPI).
- Thư viện MQTT: PubSubClient, JSON: ArduinoJson
- Thời gian: NTP, múi giờ Asia/Ho_Chi_Minh

## Lệnh thường dùng

`pio` không có trong PATH, dùng `~/.platformio/penv/bin/pio`.

- Test parser trên máy tính (Unity): `pio test -e native`
- Test script Python: `python3 -m unittest discover -s tools` (trên Windows không có `python`: dùng
  `~/.platformio/penv/Scripts/python.exe`; cũng không có `gcc` nên `pio test -e native` không chạy được)
- API cảnh báo + vị trí trên Pi 5: `ALERTS_API_TOKEN=... sudo -E python3 tools/alerts_api.py --bind <IP LAN>
  --config alerts.json` (mẫu `tools/alerts.example.json`, chi tiết `docs/pi5_api.md`)
- Script Pi 5: `sudo python3 tools/pi5_stats_publisher.py --host <broker> --services nginx,mosquitto,docker,node-red`
- Giả lập công tắc (không cần thiết bị thật): `python3 tools/home_sim.py --host <broker> [--drop 3]`.
  `--drop N` bỏ qua lệnh thứ N để thử hết thời gian chờ.

## Cấu trúc thư mục và các tầng

Chỉ được phụ thuộc **xuống dưới** (kiểm tra bằng `grep` trước khi commit):

```
app       src/app/app.*        khởi tạo theo thứ tự, tạo mọi task (stack + priority chỉ ở app.cpp)
ui        src/ui/              ui_manager, theme, components/, screens/   ← chỉ tầng này gọi lv_*
services  src/services/        wifi, time, mqtt, pi5, home, weather,     ← không include ui/, không lv_*
                               settings, location, alerts (+ pi5_api: HTTP tới Pi 5)
core      src/core/            state, events, config, models, parsers    ← không include services/ui/hal
hal       src/hal/             display, touch, backlight, button         ← chỉ biết phần cứng + driver LVGL
```

- `ui` không include `hal/` hay `board_pins.h` (kích thước màn lấy bằng `lv_display_get_*` / `LV_PCT`).
- Thứ tự khởi tạo (`app_init`): `state`/`events` → `hal` (button, display + LVGL, backlight, touch) →
  `settings` (NVS → độ sáng + đơn vị) → `location` (bản NVS hoặc mặc định) → `ui_init` →
  `wifi` → `time` → `mqtt` → `pi5`, `home`, `weather` (đăng ký route MQTT trước khi task `mqtt` chạy) → `alerts`
  → tạo task.
- `main.cpp` chỉ gọi `app_init()`. Include theo dạng `#include "services/wifi_service.h"` (`-I src`).
- `include/board_pins.h`: chân GPIO, kích thước panel (chỉ `hal/` dùng). `include/app_config.h`: topic, chu kỳ,
  thời gian chờ, vị trí mặc định (`DEFAULT_CITY/LAT/LON`, chỉ dùng khi Pi 5 chưa trả lời lần nào).
- `core/config.*`: **file duy nhất include `secrets.h`** (`config_wifi_ssid()`, `config_mqtt_host()`,
  `config_pi5_api_host()`, ...) và đọc/ghi NVS: mạng đã lưu (`wifi_cfg`), cài đặt (`settings`: `bright`,
  `sleep_s`, `unit`), vị trí gần nhất (`location`).
- `core/models.h`, `core/parsers.*`: struct và hàm parse JSON/topic thuần (không phụ thuộc Arduino/LVGL).
  Env `native` chỉ biên dịch `core/parsers.cpp`; parse mới thì thêm test ở `test/test_parsers/`.
- `services/ca_certs.h`: CA gốc ISRG Root X1 cho Open-Meteo (hết hạn 2035).
- Không đặt tên file `wifi.h`: macOS không phân biệt hoa thường nên sẽ che mất `<WiFi.h>` (dùng `wifi_service.*`).
- `include/secrets.h`: Wi-Fi, MQTT, mật khẩu AP `SETUP_AP_PASSWORD`, API Pi 5 (`PI5_API_HOST/PORT/TOKEN`;
  thiếu thì build vẫn được, cảnh báo/vị trí tắt) (không commit, không đọc). Mẫu: `include/secrets.example.h`
- Topic MQTT chi tiết: `docs/mqtt_topics.md`. Luồng Wi-Fi: `.claude/rules/wifi.md`. Màn 3: `.claude/rules/home.md`.
- **Thêm tab mới** = 1 service (nếu có dữ liệu mới: struct + nhóm `StateGroup` trong `core/state`) + 1 `screen_*`
  (`create` + `poll`) + 1 dòng trong `kTiles` (`ui_manager.cpp`). **Thêm topic** = 1 handler + `mqtt_service_route()`.

## Dữ liệu (MQTT)

Một client MQTT duy nhất (`services/mqtt_service.cpp`): chỉ lo kết nối, publish qua hàng đợi và định tuyến.
Các service đăng ký `mqtt_service_route(filter, handler)` trong `*_init()`; mọi filter được subscribe mỗi lần
kết nối, tin nhắn đi tới route đầu tiên khớp (`mqtt_topic_matches`, hỗ trợ `+`/`#`). Buffer 4096 byte cho `home/config`.

| Topic                           | Hướng                               | Payload                                                                                         |
| ------------------------------- | ----------------------------------- | ----------------------------------------------------------------------------------------------- |
| `pi5/stats`                     | nhận, mỗi 2 giây                    | `{"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}`                                    |
| `pi5/services`                  | nhận, retain, khi đổi + mỗi 10 giây | `{"services":[{"name":"nginx","status":"running","port":80}]}`; `port` có thể `null`, tối đa 10 |
| `home/config`                   | nhận, retain                        | `{"rooms":[{"id","name","panels":[{"id","name","switches":[{"id","name","icon","state"}]}]}]}`  |
| `home/<phòng>/<cụm>/<số>/state` | nhận                                | `ON` / `OFF`                                                                                    |
| `home/<phòng>/<cụm>/<số>/set`   | gửi                                 | `ON` / `OFF`                                                                                    |

- `status` khác `running` được coi là `stopped`. JSON hỏng thì bỏ qua và giữ dữ liệu cũ.
- id phòng hoặc cụm chứa `/`, `+`, `#` sẽ bị bỏ qua. Giới hạn: 8 phòng, 6 cụm mỗi phòng.
- Thời tiết: Open-Meteo qua HTTPS cho `lat`/`lon` của Pi 5 (`location_service`; vị trí đổi thì tải lại ngay)
  (kiểm tra CA, nên phải chờ NTP), làm mới mỗi 10 phút; lỗi thì thử lại sau
  30 s, 60 s, 120 s... tối đa 10 phút. **Phải `http.useHTTP10(true)`**: với HTTP/1.1 Open-Meteo trả chunked,
  `getStream()` đưa cả dòng độ dài chunk (`142\r\n{...`) và ArduinoJson đọc nó như số `142`, "thành công" mà không có
  dữ liệu. Lỗi giữ dữ liệu cũ kèm `Updated N min ago`; chưa có dữ liệu thì `Weather unavailable` + lý do.

## Dữ liệu (REST trên Pi 5, task 8)

HTTP thường trong LAN tới `tools/alerts_api.py`, header `Authorization: Bearer <PI5_API_TOKEN>`, timeout 5 s,
keep-alive (`services/pi5_api.*`). Chi tiết: `docs/pi5_api.md`.

- `GET /api/alerts/summary` mỗi 20 s → chấm đỏ của chuông; lỗi thì lùi 30 s → 5 phút; mất mạng giữ giá trị cuối.
- `GET /api/alerts` (tối đa 10, mới nhất trước) chỉ khi màn Alerts mở: lúc mở, mỗi 20 s, hoặc khi `latest_id`
  đổi. `POST /api/alerts/ack-all` cho `Clear all`: chỉ xóa danh sách khi nhận 200.
- `GET /api/location` lúc khởi động, khi có lại mạng, mỗi 5 phút → nhãn tab Weather, hàng Location, tọa độ thời tiết.
- Mọi chuỗi từ Pi 5 qua `ascii_sanitize()`: giữ ASCII, `°`, `•` (`·` thành `•`), ký tự khác thành `?`
  (font Montserrat dựng sẵn không có tiếng Việt).

## Quy ước giao diện (480x272, nền tối, phẳng)

- Màu: nền `#0B0F14`, thẻ `#151B23`, track `#263140`, chữ `#E6EDF3`, chữ phụ `#8B98A8`
- CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524` (đỏ khi > 70°C)
- Xanh kết nối `#3DDC84`, xám mất kết nối `#6B7788`, đỏ `#FF6B6B`
- Font Montserrat 12 / 14 / 16 / 20 / 48 (`FONT_S/M/MD/L/XL`). Không gradient, không đổ bóng.
  LVGL không có Montserrat đậm: các `FONT_*_BOLD` tạm trỏ về font thường cùng cỡ.
- Padding ngoài 10px, khoảng cách 8px, bo góc thẻ 12.
- Thanh trạng thái (28px, font 14): chấm 8px + nhãn, 3 chấm chỉ trang (đang chọn 20x4, còn lại 8x4),
  chuông + bánh răng (khung 28px, vùng chạm 40px), icon Wi-Fi 20px, giờ. Chạm icon Wi-Fi khi chưa kết nối thì mở
  lại màn cấu hình. Chấm: màn 1 và 3 là MQTT, màn 2 là lần lấy thời tiết gần nhất. Nhãn màn 2 = thành phố của Pi 5.
  Chuông có chấm đỏ `#FF6B6B` 10px (viền `#0B0F14`) khi `unacked > 0`.
- Màn phủ (`ui/components/overlay.*`): `lv_screen` riêng, thanh trên 36px `< Back` + tiêu đề. Back về tab đã rời.
  Alerts xóa widget dòng khi rời màn (`lv_obj_clean`), dữ liệu giữ trong state.
- Mức cảnh báo khác cả hình lẫn màu (`ui/components/alert_icon.*`): tròn đặc đỏ `!` (critical), tam giác vàng
  `!` (warning), vuông bo viền tím `i` (info).
- Tắt màn hình: `ui_manager` đọc `lv_display_get_inactive_time()`; giảm sáng 5 s trước khi tắt. Khi tối/tắt có một
  lớp trong suốt trên `lv_layer_top()` nuốt lần chạm đầu (chỉ đánh thức); nút BOOT khi tối chỉ đánh thức.
- Nhiệt độ: dữ liệu luôn °C, đổi đơn vị chỉ khi hiển thị (`temp_in_unit()`); ngưỡng nóng 70 °C giữ theo °C.
- Màn 1: 3 đồng hồ cao 84px (arc 56, nét 6); danh sách service dòng 24px (vừa 4 dòng), cột Status và Port
  rộng 72px.
- Màn 2: thẻ chính 116px (icon 84, nhiệt độ font 48, cột phải Humidity / Wind / Rain chance), 5 thẻ dự báo.
  `Rain chance` = `precipitation_probability` của giờ đang diễn ra.
- Danh sách service: cột Status chỉ có chấm (đặc xanh = running, vòng rỗng đỏ `#FF6B6B` = stopped).
- Danh sách cuộn dọc: container phải bật CLICKABLE (LVGL tìm đối tượng cuộn từ đối tượng được chạm
  đi lên cha, còn `ui_box()` tắt CLICKABLE), và chỉ đặt `LV_DIR_VER` để vuốt ngang vẫn đổi màn.
  Thanh cuộn 3px `#3A4656` qua `ui_style_scrollbar()` (`ui/theme.h`).

## Quy tắc code

- Chỉ tầng `ui` gọi LVGL, và chỉ từ task `lvgl` (`ui_task`, nơi duy nhất gọi `lv_timer_handler()`).
  `hal/display` và `hal/touch` được gọi API driver LVGL lúc khởi tạo. Chỉ task `mqtt` dùng PubSubClient.
- **Dữ liệu đi qua state store** (`core/state.*`), không qua hàng đợi: service ghi `state_set_*()`, mỗi nhóm có
  `state_version()`; screen chỉ sao chép khi version đổi (`screen_*_poll()`, gọi bởi `lv_timer` 250 ms và mỗi
  khi có event). `core/events` chỉ là tín hiệu đánh thức không mang dữ liệu: mất một event không mất dữ liệu
  (trước đây mất một sự kiện Wi-Fi là icon sai mãi).
- Task `wifi` đọc lại `WiFi.status()` + IP mỗi 250 ms và đồng bộ cả `BIT_CONNECTED`, vì sự kiện `DISCONNECTED`
  có thể đến sau `GOT_IP`.
- Lệnh từ giao diện đi qua hàm của service (`home_service_set()`, `wifi_service_command()`): chỉ đưa vào hàng
  đợi hoặc state, không chặn. Logic chờ `.../state` và hết hạn 3 s nằm trong `home_service`, không ở UI.
- Mỗi service: một cặp `.h/.cpp`, `xxx_init()` + `xxx_task()` (nếu chạy nền), chi tiết nội bộ để `static`.
- Bản sao ~7 KB (`HomeConfig`/`HomeState`) cấp trong PSRAM bằng `state_alloc_large()` để chừa RAM nội cho TLS.
- Việc mạng chạy trong task FreeRTOS riêng, không chặn giao diện.
- Không hard-code Wi-Fi hay mật khẩu; đọc qua `core/config.h` (lấy từ `include/secrets.h`).
- Sau khi sửa code phải chạy `pio run` và đảm bảo build thành công; sửa parser thì chạy `pio test -e native`.
- Giữ thay đổi nhỏ, đúng phạm vi yêu cầu; không refactor ngoài yêu cầu.
- Không commit thay đổi `.vscode/extensions.json` do extension PlatformIO tự sửa.
