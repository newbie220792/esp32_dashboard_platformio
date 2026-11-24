# Task 4: Chuyển sang board JC4827W543C (480x272), đổi UI và thư viện

Thiết kế tham chiếu (6 artboard, khổ 480x272): https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd

Task này **thay thế mọi kích thước 320x240 và thư viện LovyanGFX** nêu trong `task.md`, `task_2.md`, `task_3.md`. Luồng chức năng, MQTT topic và JSON của các task trước **giữ nguyên**; chỉ khác kích thước giao diện và lớp driver.

## 1. Phần cứng: Guition JC4827W543C

- MCU: ESP32-S3 (module N4R8): **4 MB flash, 8 MB PSRAM**, Wi-Fi, BLE 5.
- Màn hình: 4.3" IPS **480x272**, driver **NV3041A** giao tiếp **QSPI (4-bit)**.
- Cảm ứng: **điện dung GT911** (I2C). Board có 3 biến thể: N (không cảm ứng), R (điện trở), C (điện dung); ta dùng bản **C**.
- Chỉ xoay được **0° hoặc 180°** (giữ màn nằm ngang, 480 rộng x 272 cao).
- Có khe thẻ SD (dùng chung bus, xem mục 6).

Chân GPIO (QSPI màn, I2C cảm ứng, đèn nền) **chưa được xác nhận trong file này**. Lấy từ repo/tài liệu chính thức của board (ví dụ repo `profi-max/JC4827W543_4.3inch_ESP32S3_board`) và ghi vào `CLAUDE.md`. Không đoán chân.

## 2. Thay đổi thư viện

| Hạng mục                | Trước                                  | Sau                                                                                                                                                                       |
| ----------------------- | -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Driver màn hình         | LovyanGFX / TFT_eSPI                   | **GFX Library for Arduino (`moononournation/GFX Library for Arduino`)** với lớp NV3041A qua bus QSPI. TFT_eSPI và LovyanGFX không dùng được vì NV3041A QSPI không có sẵn. |
| Cảm ứng                 | XPT2046 / chưa chọn                    | **GT911** qua I2C (ví dụ thư viện `TAMC_GT911` hoặc `bb_captouch`)                                                                                                        |
| LVGL                    | 9.x                                    | 9.x, **cấu hình lại cho 480x272** (xem mục 3)                                                                                                                             |
| MQTT, JSON, WiFiManager | PubSubClient, ArduinoJson, WiFiManager | **Giữ nguyên**                                                                                                                                                            |
| Phương án thay thế      |                                        | Có thể dùng thư viện `rzeldent/esp32-smartdisplay` nếu nó hỗ trợ đúng board này; kiểm tra trước khi chọn, nếu có thì nó gói sẵn màn, cảm ứng và LVGL                      |

`platformio.ini` gợi ý (kiểm tra lại tên board, `memory_type` và phiên bản thư viện khi build):

```ini
[env:jc4827w543c]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.flash_size = 4MB
board_build.arduino.memory_type = qio_opi   ; PSRAM octal của N4R8, kiểm tra lại
board_build.partitions = huge_app.csv       ; flash chỉ 4 MB, cần partition lớn
build_flags =
    -DBOARD_HAS_PSRAM
    -DLV_CONF_INCLUDE_SIMPLE
    -DARDUINO_USB_CDC_ON_BOOT=1
    -I include
lib_deps =
    lvgl/lvgl@^9.2.0
    moononournation/GFX Library for Arduino
    tzapu/WiFiManager
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    ; thư viện GT911: chọn một (TAMC_GT911 hoặc bb_captouch)
```

## 3. Cấu hình LVGL cho 480x272

- Độ phân giải: `lv_display_create(480, 272)`, `LV_COLOR_DEPTH 16`, byte order phù hợp với driver (xem mục 6).
- Bộ đệm vẽ (draw buffer): đặt trong **PSRAM** bằng `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`, kích thước tối thiểu 1/10 màn hình (480 x 27 x 2 byte) hoặc tốt hơn là 1/4 màn hình, dùng **double buffer** nếu RAM cho phép.
- Bật font Montserrat **12, 14, 16, 20, 48** trong `lv_conf.h`; tắt các font không dùng để tiết kiệm flash 4 MB.
- Tắt widget LVGL không dùng (ví dụ `LV_USE_CHART` vì đã bỏ biểu đồ, `LV_USE_CALENDAR`, ...).
- Đăng ký input device cảm ứng (GT911) cho LVGL với tọa độ 480x272.
- Gọi LVGL chỉ từ một task (hoặc mutex); callback MQTT và Wi-Fi đẩy dữ liệu qua queue.

## 4. Thay đổi giao diện theo kích thước mới

Màn hình **480x272**, nền `#0B0F14`, thẻ `#151B23` bo góc 12, padding ngoài 10px, khoảng cách 8px. Thanh trạng thái cao 28px, font 14. Màu sắc không đổi (CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524`, xanh `#3DDC84`, xám `#6B7788`, đỏ `#FF6B6B`).

### Thanh trạng thái (cả 3 màn chính)

Trái: chấm 8px + nhãn (`Pi 5 MQTT`, `Ho Chi Minh`, `Home MQTT`). Giữa: 3 chấm chỉ trang (đang chọn 20x4 `#3DD6C6`, còn lại 8x4 `#3A4656`). Phải: icon Wi-Fi 20px (xanh khi kết nối, xám khi mất) + giờ.

### Màn 1: Pi 5 Monitor

- Hàng 3 đồng hồ cao **84px**: `lv_arc` 56x56, nét 6, cung 270 độ; giá trị trong cung font 14 (`37%`, `52%`, `58°`); nhãn dưới font 12 (`CPU`, `RAM 4.1/8G`, `TEMP`). TEMP đỏ khi > 70°C.
- Danh sách service chiếm phần còn lại (khoảng 124px), thẻ padding 6x16:
    - Hàng tiêu đề cao 16px, font 12 (`Service` / `Status` / `Port`).
    - Mỗi dòng cao **24px**, font 14, hiển thị vừa **4 dòng**, cuộn dọc khi nhiều hơn, thanh cuộn 3px `#3A4656`.
    - Cột `Status` rộng 72px chỉ có chấm 8px (đặc xanh = `running`, vòng rỗng viền 2px đỏ = `stopped`), không có chữ. Cột `Port` rộng 72px căn phải.

### Màn 2: Weather

- Thẻ chính cao **116px**: icon 84px, nhiệt độ **font 48**, tình trạng (16 đậm), `Feels like` (12); cột phải 3 chỉ số `Humidity`, `Wind`, `Rain chance` (nhãn 12, giá trị 16 đậm).
- Dưới: 5 thẻ dự báo (Now + 4 mốc giờ) chia đều, mỗi thẻ gồm giờ (12), icon 28px, nhiệt độ (16 đậm). `Now` nền `#3DD6C6`, chữ tối.
- `Rain chance` lấy từ trường xác suất mưa của Open-Meteo (`precipitation_probability`).

### Màn 3: Home switches

- Panel phòng nền `#151B23`, padding 6. Hàng tiêu đề cao 32px: nút `‹` `›` rộng 44x32 (nền `#1D2733`), tên phòng font 20 đậm + `1/4` (12, `#8B98A8`).
- Mỗi **panel cụm** nền `#1D2733`, bo góc 10, padding 4, cao khoảng **82px**, có nhãn (12) và hàng 3 công tắc. Vừa **2 cụm**; nhiều hơn thì cuộn dọc (hàng tiêu đề đứng yên).
- **Công tắc**: bo góc 10, cao khoảng 58px, rộng khoảng 140px; icon 22px, tên font 14 đậm, trạng thái `ON`/`OFF` font 12. ON: nền `#F5A524`, chữ và icon `#0B0F14`. OFF: nền `#2A3646`, tên `#E6EDF3`, trạng thái và icon `#A3AEBB`.
- Vẫn chỉ đổi màu ô khi nhận phản hồi `.../state` (xem `task_2.md`).

### Màn 4, 5, 6: Wi-Fi (xem `task_3.md` về luồng)

- Thanh trên cao **36px**: nút `‹ Back` cao 32px (nền `#151B23`, font 14 đậm, bo góc 10), nhãn màn, icon Wi-Fi xám bên phải. `Back` quay về màn 1.
- **Màn 4:** icon Wi-Fi gạch chéo đỏ 44px, tiêu đề `Can't connect` (font 20 đậm), `Network: <SSID>` (14), lý do (14 đậm, đỏ), thanh đếm ngược 4px, 2 nút cao **44px** font 16 (`Retry` nền `#2A3646`, `Setup now` nền `#3DD6C6` chữ tối).
- **Màn 5:** khung QR **136x136** (`lv_qrcode`) bên trái; bên phải `Join this network` / `ESP32-Setup` (20 đậm, `#3DD6C6`) / `Then open` / `192.168.4.1` (20 đậm) / gợi ý (12); dưới là thẻ trạng thái 36px `Waiting for setup…` (chấm vàng).
- **Màn 6:** icon gạch chéo đỏ 32px, `Couldn't connect`, tên mạng, lý do; thẻ thông tin 52px (`Setup is open again`, `Attempt N`, `ESP32-Setup · 192.168.4.1`); thẻ trạng thái 36px.

## 5. Việc cần làm

1. Lấy sơ đồ chân và cấu hình driver của board từ repo/tài liệu chính thức; ghi vào `CLAUDE.md` mục "Phần cứng".
2. Cập nhật `platformio.ini` theo mục 2; thay LovyanGFX/TFT_eSPI bằng GFX Library for Arduino, thêm thư viện GT911.
3. Viết lớp hiển thị (display driver) và nối cảm ứng vào LVGL (flush callback, read callback) cho 480x272.
4. Cập nhật `lv_conf.h` theo mục 3; đặt draw buffer vào PSRAM.
5. Chỉnh toàn bộ UI (`ui_monitor.*`, `ui_weather.*`, `ui_home.*`, `ui_wifi.*`) theo kích thước ở mục 4.
6. Kiểm tra dung lượng flash sau khi build; nếu vượt thì giảm font, tắt module LVGL không dùng, chọn lại partition.

## 6. Lưu ý đã biết với board này

- Driver NV3041A cần cài đặt **đảo màu (invert)** và **đổi thứ tự byte (byte swap)** thì màu mới đúng. Nếu màu bị sai thì thử đổi hai tùy chọn này trước.
- Chỉ xoay được 0° hoặc 180°; không dùng xoay 90°/270°.
- Khi dùng GFX Library for Arduino, thẻ SD và cảm ứng có thể lỗi sau khi đẩy khung hình do dùng chung bus. Cần sắp xếp thứ tự thao tác, và kiểm tra cảm ứng hoạt động liên tục sau nhiều lần vẽ.
- Chỉ 4 MB flash, không có phiên bản 16 MB: build lớn có thể không đủ chỗ.
- Cần dùng PSRAM cho draw buffer; RAM nội của ESP32-S3 không đủ cho bộ đệm lớn.
- Với board này hiện tai không dùng SD card
- Cố định độ sánh nhất cho đèn nền
- Dùng GFX Library for Arduino trực tiếp cho driver màn hình

## 7. Tiêu chí hoàn thành

- Màn hình hiển thị đúng màu, không bị ngược hoặc sai màu, ở 480x272 nằm ngang.
- Chạm cảm ứng GT911 hoạt động ổn định, kể cả sau khi vẽ nhiều khung hình liên tục.
- Cả 6 màn hình đúng với thiết kế: kích thước, font, màu, vùng chạm (nút tối thiểu 32px cao, công tắc khoảng 58px cao).
- Danh sách service cuộn mượt, hiển thị 4 dòng; màn 3 hiển thị 2 cụm.
- Luồng Wi-Fi trong `task_3.md` vẫn chạy đúng, gồm cả nút `Back`.
- Tốc độ vẽ lại mượt (không giật rõ khi cuộn danh sách và đổi màn).
- `pio run` build thành công, kích thước firmware nằm trong partition đã chọn, không có warning mới.
