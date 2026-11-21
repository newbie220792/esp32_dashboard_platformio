# ESP32 LVGL – Pi 5 Monitor, Weather & Home switches

Firmware ESP32 hiển thị giao diện LVGL 9 gồm 3 màn hình vuốt ngang (`lv_tileview`):

1. **Pi 5 Monitor**: CPU %, RAM %, nhiệt độ và danh sách service của Raspberry Pi 5 qua MQTT.
2. **Weather**: thời tiết thời gian thực (Open-Meteo, không cần API key).
3. **Home switches**: điều khiển công tắc nhà theo phòng → cụm → 3 công tắc qua MQTT.

Thiết kế tham chiếu: https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd
Đặc tả công việc: `tasks/`.

## Phần cứng

- Board: ESP32-S3 (esp32-s3-devkitc-1, flash 4MB)
- Màn hình: ILI9341 320x240 nằm ngang, SPI. Chân mặc định trong `include/app_config.h`:
  SCLK=12 MOSI=11 MISO=13 CS=10 DC=9 RST=14 BL=21 (cần kiểm tra lại với mạch thật)
- Cảm ứng: GT911 (I2C, thư viện TouchLib) SDA=8 SCL=4 RST=38 INT=3. Hiệu chỉnh bằng `TOUCH_DEBUG`
  và `TOUCH_RAW_W/H`, `TOUCH_SWAP_XY`, `TOUCH_INVERT_X/Y`.
- Nút BOOT (GPIO0): bấm nhanh để chuyển màn; giữ 5 giây để xóa Wi-Fi đã lưu và mở chế độ cấu hình.

## Công nghệ

- PlatformIO + Arduino framework
- LVGL 9.x, LovyanGFX (driver màn hình), TouchLib (cảm ứng), WiFiManager (cổng cấu hình Wi-Fi)
- Thư viện MQTT: PubSubClient, JSON: ArduinoJson
- Thời gian: NTP, múi giờ Asia/Ho_Chi_Minh

## Lệnh thường dùng

`pio` không có trong PATH, dùng `~/.platformio/penv/bin/pio`.

- Build: `pio run` (mặc định env `esp32s3`)
- Nạp firmware: `pio run -t upload`
- Xem log serial: `pio device monitor`
- Test parser trên máy tính (Unity): `pio test -e native`
- Test script Python: `python3 -m unittest discover -s tools`
- Script Pi 5: `sudo python3 tools/pi5_stats_publisher.py --host <broker> --services nginx,mosquitto,docker,node-red`
- Giả lập công tắc (không cần thiết bị thật): `python3 tools/home_sim.py --host <broker> [--drop 3]`.
  `--drop N` bỏ qua lệnh thứ N để thử hết thời gian chờ.

## Cấu trúc thư mục

- `src/main.cpp`: khởi tạo, task LVGL
- `src/ui.*`: màn hình gốc, thanh trạng thái, tileview, phân phối sự kiện từ hàng đợi
- `src/ui_monitor.*`: màn 1 (3 đồng hồ nhỏ + danh sách service)
- `src/ui_weather.*`, `src/weather_icon.*`: màn 2 (thời tiết)
- `src/ui_home.*`: màn 3 (panel phòng, panel cụm, công tắc)
- `src/ui_theme.*`: màu, font, helper (`ui_box`, `ui_card`, `ui_style_scrollbar`)
- `src/icon_draw.*`: hàm vẽ icon vector dùng chung (không dùng ảnh bitmap)
- `src/models.h`, `src/parsers.*`: struct dữ liệu và hàm parse JSON/topic thuần (không phụ thuộc
  Arduino/LVGL). Env `native` chỉ biên dịch `parsers.cpp`; parse mới thì thêm test ở `test/test_parsers/`.
- `src/app_events.*`: hàng đợi từ task mạng sang task LVGL
- `src/wifi_setup.*`: luồng cấu hình Wi-Fi lúc khởi động (máy trạng thái, WiFiManager, NVS)
- `src/ui_wifi.*`: các màn Wi-Fi (Connecting, màn 4, 5, 6) trên một screen LVGL riêng
- `src/wifi_manager.*`, `src/mqtt.*`, `src/weather.*`: mạng và dữ liệu
  (không đặt tên `wifi.h`: macOS không phân biệt hoa thường nên sẽ che mất `<WiFi.h>`)
- `src/display.cpp`, `src/touch.cpp`: driver màn hình và cảm ứng
- `include/app_config.h`: chân, topic, vị trí, chu kỳ, thời gian chờ
- `include/lv_conf.h`: cấu hình LVGL
- `include/secrets.h`: Wi-Fi, MQTT, mật khẩu AP `SETUP_AP_PASSWORD` (không commit, không đọc). Mẫu: `include/secrets.example.h`
- `tools/`: script Python cho Pi 5 và giả lập công tắc

## Wi-Fi

Task `wifi` (core 0) chạy `wifi_setup_run()` cho đến khi kết nối được, sau đó mới giám sát kết nối lại và cài NTP.
Mỗi lần đổi trạng thái được gửi qua `AppEventType::WifiSetup`; `ui.cpp` hiện screen Wi-Fi và chỉ chuyển sang
giao diện chính (tileview) khi nhận `Connected`.

1. `CONNECTING`: thử `secrets.h` trước (**`secrets.h` luôn thắng**), nếu thất bại thì thử mạng đã lưu trong NVS
   (namespace `wifi_cfg`, chỉ chứa mạng nhập qua portal). Timeout `WIFI_CONNECT_TIMEOUT_MS` (18 giây) mỗi mạng;
   dừng sớm khi cùng một lỗi rõ ràng (không thấy mạng, sai mật khẩu) lặp lại 3 lần.
2. `FAILED_SAVED` (màn 4): báo lý do của mạng `secrets.h`, đếm ngược 5 giây rồi mở portal. `Retry` thử lại bước 1,
   `Setup now` mở portal ngay.
3. `PORTAL` (màn 5): AP `ESP32-Setup` (mật khẩu `SETUP_AP_PASSWORD`, trống thì là mạng mở) + captive portal tại
   `192.168.4.1`. WiFiManager chạy không chặn (`setConfigPortalBlocking(false)`, `process()` mỗi 10 ms).
4. `CONNECTING_NEW`: báo từ `setPreSaveConfigCallback`, đọc SSID và mật khẩu từ `server->arg("s"/"p")`, tăng `Attempt`.
   Lần thử kết nối chạy đồng bộ bên trong `process()` (chỉ chặn task `wifi`, giao diện vẫn chạy).
5. Thành công: lưu vào NVS, tắt portal, `Connected`. Thất bại: `FAILED_NEW` (màn 6), portal vẫn mở, lặp lại không giới hạn.

- Lý do lỗi lấy từ `reason` của `WIFI_EVENT_STA_DISCONNECTED`, dự phòng bằng `WiFi.status()`
  (`wifi_fail_reason()` trong `parsers.cpp`, có test).
- Mã QR: `build_wifi_qr()` (`WIFI:T:WPA;S:..;P:..;;` hoặc `T:nopass`, có escape ký tự đặc biệt).
- Giữ BOOT 5 giây (`WifiCmd::ResetAndPortal`): xóa NVS rồi mở portal, kể cả khi đang ở giao diện chính.
- Sau khi đã vào giao diện chính, mất Wi-Fi chỉ làm icon chuyển xám và tự kết nối lại, không quay về màn cấu hình.
- `WiFi.persistent(false)`: thông tin Wi-Fi chỉ nằm trong NVS của mình.

## Dữ liệu (MQTT)

Một client MQTT duy nhất (`mqtt.cpp`). Bảng `kRoutes` vừa để subscribe vừa để phân phối theo topic
(`mqtt_topic_matches`, hỗ trợ `+`/`#`). Buffer 4096 byte cho `home/config`.

| Topic | Hướng | Payload |
|---|---|---|
| `pi5/stats` | nhận, mỗi 2 giây | `{"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}` |
| `pi5/services` | nhận, retain, khi đổi + mỗi 10 giây | `{"services":[{"name":"nginx","status":"running","port":80}]}`; `port` có thể `null`, tối đa 10 |
| `home/config` | nhận, retain | `{"rooms":[{"id","name","panels":[{"id","name","switches":[{"id","name","icon","state"}]}]}]}` |
| `home/<phòng>/<cụm>/<số>/state` | nhận | `ON` / `OFF` |
| `home/<phòng>/<cụm>/<số>/set` | gửi | `ON` / `OFF` |

- `status` khác `running` được coi là `stopped`. JSON hỏng thì bỏ qua và giữ dữ liệu cũ.
- id phòng hoặc cụm chứa `/`, `+`, `#` sẽ bị bỏ qua. Giới hạn: 8 phòng, 6 cụm mỗi phòng.
- Thời tiết: Open-Meteo, làm mới mỗi 10 phút.

## Quy ước giao diện (320x240, nền tối, phẳng)

- Màu: nền `#0B0F14`, thẻ `#151B23`, track `#263140`, chữ `#E6EDF3`, chữ phụ `#8B98A8`
- CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524` (đỏ khi > 70°C)
- Xanh kết nối `#3DDC84`, xám mất kết nối `#6B7788`
- Font Montserrat 12 / 14 / 20 / 40. Không gradient, không đổ bóng.
  LVGL không có Montserrat đậm: `FONT_S_BOLD` tạm trỏ về font 12 thường.
- Padding 8px, khoảng cách 6px, bo góc thẻ 10.
- Thanh trạng thái (20px): chấm trạng thái + nhãn, 3 chấm chỉ trang, icon Wi-Fi, giờ.
  Chấm: màn 1 và 3 là MQTT, màn 2 là lần lấy thời tiết gần nhất.
- Danh sách service: cột Status chỉ có chấm (đặc xanh = running, vòng rỗng đỏ `#FF6B6B` = stopped).
- Danh sách cuộn dọc: container phải bật CLICKABLE (LVGL tìm đối tượng cuộn từ đối tượng được chạm
  đi lên cha, còn `ui_box()` tắt CLICKABLE), và chỉ đặt `LV_DIR_VER` để vuốt ngang vẫn đổi màn.
  Thanh cuộn 3px `#3A4656` qua `ui_style_scrollbar()`.

### Màn 3: Home switches

- Panel phòng `#151B23` (bo 10, padding 6). Hàng tiêu đề 24px: `‹` 32px, tên phòng + `1/4`, `›` 32px.
  Đổi phòng vòng tròn. Nhận `home/config` mới thì giữ phòng đang xem nếu id còn tồn tại.
- Panel cụm `#1D2733` (bo 8, padding 4, cao 80px): nhãn cụm 14px + hàng 3 công tắc. Vừa 2 cụm, nhiều
  hơn thì cuộn (tiêu đề đứng yên).
- Mỗi cụm luôn có đúng 3 ô: thiếu thì để ô trống, thừa thì bỏ.
- Công tắc: ON nền `#F5A524`, chữ và icon `#0B0F14`. OFF nền `#2A3646`, tên `#E6EDF3`, trạng thái
  và icon `#A3AEBB`. Icon theo trường `icon`: `bulb`, `fan`, `plug`, `snow` (hoặc `ac`).
- Chạm: gửi trạng thái ngược lại lên `.../set` và làm mờ ô 40% kèm chữ `...`. **Chỉ đổi màu khi
  nhận `.../state`.** Sau `SWITCH_PENDING_MS` (3 giây) không có phản hồi thì trả về trạng thái cũ.
- Mất MQTT: ô mờ 50%, bỏ qua thao tác chạm, xóa các lệnh đang chờ.

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
