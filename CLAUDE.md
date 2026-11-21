# ESP32 LVGL – Pi 5 Monitor, Weather & Home switches

Firmware ESP32 hiển thị giao diện LVGL 9 gồm 3 màn hình vuốt ngang (`lv_tileview`):

1. **Pi 5 Monitor**: CPU %, RAM %, nhiệt độ và danh sách service của Raspberry Pi 5 qua MQTT.
2. **Weather**: thời tiết thời gian thực (Open-Meteo, không cần API key).
3. **Home switches**: điều khiển công tắc nhà theo phòng → cụm → 3 công tắc qua MQTT.

Thiết kế tham chiếu: https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd
Đặc tả công việc: `tasks/`.

## Phần cứng: Guition JC4827W543C (4.3 inch, cảm ứng điện dung)

- MCU: ESP32-S3-WROOM-1 N4R8 (flash 4 MB, PSRAM 8 MB octal). Mô tả của người bán ghi "LX6" là sai.
  `board = esp32-s3-devkitc-1` + `board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM`;
  thiếu `qio_opi` thì không có PSRAM (draw buffer tự lùi về 20 dòng trong RAM nội, chậm hơn).
- Màn hình: IPS 480x272, driver **NV3041A qua QSPI**. Chân trong `include/app_config.h`
  (lấy từ cấu hình ESPHome của board): CS=45 CLK=47 D0=21 D1=48 D2=40 D3=39, đèn nền BL=1 bật cố định
  sáng nhất. Không có chân reset màn hình.
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
- Flash chỉ 4 MB (`huge_app.csv`, 3 MB cho app): chỉ bật font và widget LVGL đang dùng.
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
- Test script Python: `python3 -m unittest discover -s tools`
- Script Pi 5: `sudo python3 tools/pi5_stats_publisher.py --host <broker> --services nginx,mosquitto,docker,node-red`
- Giả lập công tắc (không cần thiết bị thật): `python3 tools/home_sim.py --host <broker> [--drop 3]`.
  `--drop N` bỏ qua lệnh thứ N để thử hết thời gian chờ.

## Cấu trúc thư mục

- `src/models.h`, `src/parsers.*`: struct dữ liệu và hàm parse JSON/topic thuần (không phụ thuộc
  Arduino/LVGL). Env `native` chỉ biên dịch `parsers.cpp`; parse mới thì thêm test ở `test/test_parsers/`.
- Không đặt tên file `wifi.h`: macOS không phân biệt hoa thường nên sẽ che mất `<WiFi.h>` (dùng `wifi_manager.*`).
- `include/secrets.h`: Wi-Fi, MQTT, mật khẩu AP `SETUP_AP_PASSWORD` (không commit, không đọc). Mẫu: `include/secrets.example.h`
- Luồng Wi-Fi chi tiết: `.claude/rules/wifi.md`. Đặc tả màn 3: `.claude/rules/home.md`.

## Dữ liệu (MQTT)

Một client MQTT duy nhất (`mqtt.cpp`). Bảng `kRoutes` vừa để subscribe vừa để phân phối theo topic
(`mqtt_topic_matches`, hỗ trợ `+`/`#`). Buffer 4096 byte cho `home/config`.

| Topic                           | Hướng                               | Payload                                                                                         |
| ------------------------------- | ----------------------------------- | ----------------------------------------------------------------------------------------------- |
| `pi5/stats`                     | nhận, mỗi 2 giây                    | `{"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}`                                    |
| `pi5/services`                  | nhận, retain, khi đổi + mỗi 10 giây | `{"services":[{"name":"nginx","status":"running","port":80}]}`; `port` có thể `null`, tối đa 10 |
| `home/config`                   | nhận, retain                        | `{"rooms":[{"id","name","panels":[{"id","name","switches":[{"id","name","icon","state"}]}]}]}`  |
| `home/<phòng>/<cụm>/<số>/state` | nhận                                | `ON` / `OFF`                                                                                    |
| `home/<phòng>/<cụm>/<số>/set`   | gửi                                 | `ON` / `OFF`                                                                                    |

- `status` khác `running` được coi là `stopped`. JSON hỏng thì bỏ qua và giữ dữ liệu cũ.
- id phòng hoặc cụm chứa `/`, `+`, `#` sẽ bị bỏ qua. Giới hạn: 8 phòng, 6 cụm mỗi phòng.
- Thời tiết: Open-Meteo, làm mới mỗi 10 phút.

## Quy ước giao diện (480x272, nền tối, phẳng)

- Màu: nền `#0B0F14`, thẻ `#151B23`, track `#263140`, chữ `#E6EDF3`, chữ phụ `#8B98A8`
- CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524` (đỏ khi > 70°C)
- Xanh kết nối `#3DDC84`, xám mất kết nối `#6B7788`, đỏ `#FF6B6B`
- Font Montserrat 12 / 14 / 16 / 20 / 48 (`FONT_S/M/MD/L/XL`). Không gradient, không đổ bóng.
  LVGL không có Montserrat đậm: các `FONT_*_BOLD` tạm trỏ về font thường cùng cỡ.
- Padding ngoài 10px, khoảng cách 8px, bo góc thẻ 12.
- Thanh trạng thái (28px, font 14): chấm 8px + nhãn, 3 chấm chỉ trang (đang chọn 20x4, còn lại 8x4),
  icon Wi-Fi 20px, giờ. Chạm icon Wi-Fi khi chưa kết nối thì mở lại màn cấu hình.
  Chấm: màn 1 và 3 là MQTT, màn 2 là lần lấy thời tiết gần nhất.
- Màn 1: 3 đồng hồ cao 84px (arc 56, nét 6); danh sách service dòng 24px (vừa 4 dòng), cột Status và Port
  rộng 72px.
- Màn 2: thẻ chính 116px (icon 84, nhiệt độ font 48, cột phải Humidity / Wind / Rain chance), 5 thẻ dự báo.
  `Rain chance` = `precipitation_probability` của giờ đang diễn ra.
- Danh sách service: cột Status chỉ có chấm (đặc xanh = running, vòng rỗng đỏ `#FF6B6B` = stopped).
- Danh sách cuộn dọc: container phải bật CLICKABLE (LVGL tìm đối tượng cuộn từ đối tượng được chạm
  đi lên cha, còn `ui_box()` tắt CLICKABLE), và chỉ đặt `LV_DIR_VER` để vuốt ngang vẫn đổi màn.
  Thanh cuộn 3px `#3A4656` qua `ui_style_scrollbar()`.

## Quy tắc code

- Chỉ gọi hàm LVGL từ task LVGL (hoặc bọc bằng mutex). Chỉ task `mqtt` được dùng PubSubClient.
- Task mạng gửi bản sao dữ liệu qua `AppEvent`. Dữ liệu lớn (`ServiceList`, `HomeConfig`) gửi bằng
  con trỏ heap: bên nhận chịu trách nhiệm `delete`; nếu `app_events_post` thất bại thì bên gửi tự xóa.
- Lệnh từ giao diện (`mqtt_request_switch`) chỉ đưa vào hàng đợi, task `mqtt` mới publish.
- Việc mạng chạy trong task FreeRTOS riêng, không chặn giao diện.
- Không hard-code Wi-Fi hay mật khẩu; dùng `include/secrets.h`.
- Sau khi sửa code phải chạy `pio run` và đảm bảo build thành công; sửa parser thì chạy `pio test -e native`.
- Giữ thay đổi nhỏ, đúng phạm vi yêu cầu; không refactor ngoài yêu cầu.
- Không commit thay đổi `.vscode/extensions.json` do extension PlatformIO tự sửa.
