# Task 5: Sửa lỗi icon Wi-Fi, lỗi tải thời tiết, và đánh giá OTA trên board 4 MB

Board: Guition JC4827W543C (ESP32-S3 N4R8: **4 MB flash**, 8 MB PSRAM, màn 480x272). Xem thêm `task_4.md`.

Task này gồm 3 phần độc lập. Làm theo thứ tự 1, 3, 2 (hai lỗi trước, OTA sau). Phần 2 **thay đổi khuyến nghị partition** `huge_app.csv` trong `task_4.md`, nên cần quyết định trước khi chốt build.

> Lưu ý: file này viết khi chưa xem code của project. Các nguyên nhân dưới đây là **giả thuyết cần kiểm chứng theo thứ tự**, không phải kết luận. Mỗi phần đều có bước thu thập log trước khi sửa.

---

## 1. Lỗi: không thấy icon Wi-Fi sau khi kết nối thành công

### Hiện tượng

Wi-Fi đã kết nối (có IP) nhưng icon Wi-Fi ở thanh trạng thái không hiện, hoặc không chuyển sang xanh `#3DDC84`.

### Giả thuyết cần kiểm tra (theo mức khả dĩ)

1. **Sự kiện đến trước khi UI tồn tại.** Icon chỉ cập nhật khi nhận sự kiện `GOT_IP`. Nếu kết nối xong trước lúc dựng giao diện (đặc biệt khi dùng `secrets.h` kết nối rất nhanh), icon không bao giờ được cập nhật. Giao diện phải **đọc trạng thái hiện tại khi khởi tạo**, không chỉ chờ sự kiện.
2. **Cập nhật LVGL từ task khác.** Callback sự kiện Wi-Fi (chạy ở task hệ thống) gọi thẳng `lv_label_set_text` / `lv_obj_set_style_*`. LVGL không an toàn đa luồng, kết quả là không vẽ lại hoặc treo ngẫu nhiên.
3. **Chỉ cập nhật một icon trong 3 màn.** Mỗi màn (Pi 5, Weather, Home) có thanh trạng thái riêng. Nếu chỉ cập nhật icon của màn đang hiển thị lúc sự kiện đến thì 2 màn còn lại vẫn sai.
4. **Icon bị ẩn hoặc sai màu.** Icon tạo với `LV_OBJ_FLAG_HIDDEN` rồi không bỏ cờ; hoặc màu xanh trùng màu nền; hoặc `lv_obj_set_style_text_color` đặt sai selector (thiếu `LV_PART_MAIN`).
5. **Thiếu glyph.** `LV_SYMBOL_WIFI` nằm trong font Montserrat dựng sẵn của LVGL; nếu label dùng font tùy chỉnh hoặc font đã tắt symbol thì chỉ hiện ô vuông trống hoặc không hiện gì.
6. **Sau luồng WiFiManager hoặc nút Back:** thanh trạng thái mới tạo lại nhưng trạng thái Wi-Fi không được nạp lại.

### Cách sửa đề xuất

- Tạo **một nguồn trạng thái duy nhất** (struct `net_state`: `wifi_connected`, `mqtt_connected`, `rssi`, `ip`) do task mạng ghi, bảo vệ bằng mutex hoặc biến nguyên tử.
- Dùng **`lv_timer` (khoảng 500 ms)** trong task LVGL để đọc `net_state` và cập nhật **mọi** icon Wi-Fi và chấm MQTT ở cả 3 màn. Callback Wi-Fi **không** gọi LVGL.
- Khi dựng thanh trạng thái, gọi hàm cập nhật ngay một lần để lấy đúng trạng thái hiện tại.
- Icon: `LV_SYMBOL_WIFI` (font Montserrat 14 hoặc 16); xanh `#3DDC84` khi kết nối, xám `#6B7788` khi không. Không dùng cờ ẩn cho icon; chỉ đổi màu.
- Khi quay lại từ màn Wi-Fi (nút Back) hoặc sau khi kết nối lại, gọi lại hàm cập nhật.

### Cách kiểm chứng

- Thêm log serial: `WiFi.status()`, `WiFi.localIP()`, và log mỗi lần timer cập nhật icon (giá trị `wifi_connected` và màu đặt).
- Thử 3 tình huống: (a) `secrets.h` đúng, kết nối ngay khi khởi động; (b) sai rồi cấu hình qua portal; (c) tắt rồi bật lại router.

### Tiêu chí hoàn thành

- Icon xanh hiện đúng ở cả 3 màn trong vòng 1 giây sau khi có IP, ở cả 3 tình huống trên.
- Mất Wi-Fi: icon chuyển xám trong vòng 2 giây; có lại Wi-Fi: chuyển xanh lại mà không cần khởi động lại.
- Không còn lệnh LVGL nào được gọi từ callback Wi-Fi hoặc MQTT.

---

## 2. Lỗi: không tải được thời tiết từ API dù Wi-Fi đã kết nối

### Hiện tượng

Có IP, ping được internet nhưng màn Weather không có dữ liệu (hoặc giữ giá trị mặc định).

### Giả thuyết cần kiểm tra (theo mức khả dĩ)

1. **TLS (HTTPS) không bắt tay được.** API Open-Meteo dùng HTTPS. `WiFiClientSecure` cần chứng chỉ gốc (CA) hoặc `setInsecure()`; không có thì `HTTPClient` trả mã âm (ví dụ `-1`). Ngoài ra cần đồng bộ giờ NTP trước khi gọi, vì kiểm tra chứng chỉ cần thời gian đúng.
2. **Thiếu RAM nội cho TLS.** Bắt tay TLS cần vùng RAM nội liền kề khá lớn (vài chục KB). Nếu LVGL/bộ đệm hiển thị chiếm RAM nội thay vì PSRAM thì cấp phát thất bại. Kiểm tra bằng `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` ngay trước khi gọi.
3. **Gọi quá sớm.** Gọi API ngay khi trạng thái là `WL_CONNECTED` nhưng trước khi có IP/DNS (`GOT_IP`) thì phân giải tên miền thất bại.
4. **DNS lỗi.** Router không cấp DNS hoặc DNS không phản hồi; nên đặt DNS dự phòng (`1.1.1.1`, `8.8.8.8`).
5. **Stack task quá nhỏ.** Task gọi HTTPS cần stack tối thiểu khoảng 8 KB (nên 10–12 KB); stack nhỏ gây reset hoặc treo mà không có thông báo rõ.
6. **Lỗi parse JSON.** Bộ nhớ `JsonDocument` quá nhỏ hoặc cấu trúc phản hồi khác dự kiến (tên trường, mảng theo giờ). Dùng bộ lọc (`filter`) và chỉ lấy trường cần.
7. **URL/tham số sai:** thiếu `latitude`, `longitude`, `timezone`, hoặc `current`/`hourly` không đúng tên.
8. **Gọi trong task LVGL** làm treo giao diện khi mạng chậm (khác với "không tải được" nhưng dễ gây hiểu nhầm là mất dữ liệu).

### Bước 1: thu thập log (bắt buộc trước khi sửa)

In ra serial khi gọi API:

- URL đầy đủ, `WiFi.localIP()`, `WiFi.dnsIP()`
- `HTTPClient::GET()` trả về mã gì (dương = mã HTTP; âm = lỗi kết nối, ví dụ `-1` không kết nối được, `-11` quá thời gian đọc)
- `ESP.getFreeHeap()`, `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`
- Kích thước phản hồi và mã lỗi của `deserializeJson`
- Thử cùng URL bằng `curl` trên máy tính để loại trừ lỗi phía API

### Cách sửa đề xuất

- Gọi API ở **task riêng** (stack 10–12 KB), không gọi trong task LVGL; kết quả đẩy sang giao diện qua queue.
- Chỉ gọi sau khi có IP và đã **đồng bộ NTP** (`configTzTime("<+07>-7", "pool.ntp.org", ...)` cho múi giờ Asia/Ho_Chi_Minh).
- Dùng `WiFiClientSecure` với CA bundle của ESP32 (hoặc tạm `setInsecure()` để xác định nguyên nhân, rồi chuyển lại CA đầy đủ). Không để `setInsecure()` ở bản chính thức nếu không cần.
- Đảm bảo bộ đệm LVGL nằm trong **PSRAM** để chừa RAM nội cho TLS.
- Đặt DNS dự phòng; thử lại với lùi dần (30 s, 60 s, 120 s, tối đa 10 phút), làm mới bình thường mỗi 10–15 phút.
- Rút gọn phản hồi bằng tham số (`current`, `hourly=temperature_2m,weather_code,precipitation_probability`, `forecast_hours`, `timezone=Asia/Ho_Chi_Minh`) và dùng `filter` của ArduinoJson.
- Khi lỗi: **giữ dữ liệu cũ** kèm nhãn "Updated N min ago"; nếu chưa từng có dữ liệu thì hiện "Weather unavailable" kèm lý do ngắn (`No network`, `Server error`, `Timeout`) ở thẻ thời tiết, không treo giao diện.

### Tiêu chí hoàn thành

- Màn Weather hiển thị đúng nhiệt độ, tình trạng, độ ẩm, gió, khả năng mưa và 4 mốc giờ tới sau khi có Wi-Fi.
- Mất mạng rồi có lại: tự tải lại, không cần khởi động lại.
- Lỗi API/mạng chỉ hiện thông báo ở thẻ thời tiết, các màn khác và cảm ứng vẫn mượt.
- Serial log ghi rõ mã lỗi mỗi lần thất bại.

---

## 3. Đánh giá OTA trên board 4 MB flash

### Kết luận ngắn

**Làm được OTA**, nhưng **không với partition `huge_app.csv`** (đã đề xuất ở `task_4.md`). `huge_app` chỉ có một vùng app lớn khoảng 3 MB, không có vùng thứ hai để ghi bản mới nên không OTA được. Để OTA qua mạng, cần **hai vùng app** (`ota_0`, `ota_1`), mỗi vùng chỉ còn **dưới 2 MB**. Điều kiện: firmware phải nhỏ hơn khoảng 1.9 MB.

### Phương án A (đề xuất): hai vùng OTA, mỗi vùng khoảng 1.94 MB

`partitions_ota.csv`:

```
# Name,    Type, SubType, Offset,   Size,     Flags
nvs,       data, nvs,     0x9000,   0x5000,
otadata,   data, ota,     0xe000,   0x2000,
app0,      app,  ota_0,   0x10000,  0x1F0000,
app1,      app,  ota_1,   0x200000, 0x1F0000,
spiffs,    data, spiffs,  0x3F0000, 0x10000,
```

Trong `platformio.ini`: `board_build.partitions = partitions_ota.csv`. Tổng đúng 4 MB (0x400000). Mỗi vùng app 0x1F0000 = 2,031,616 byte (khoảng 1.94 MB).

- Ưu điểm: OTA an toàn (nếu ghi lỗi thì vẫn chạy vùng cũ), cập nhật được qua mạng.
- Nhược điểm: firmware phải nằm trong khoảng 1.9 MB.

### Phương án B: nếu firmware vượt 1.9 MB

- Giảm kích thước: tắt font/widget LVGL không dùng (chỉ giữ Montserrat 12, 14, 16, 20, 48), bỏ `LV_USE_CHART` và các module khác, build `-Os`, tắt log debug, bỏ thư viện không dùng.
- Nếu vẫn quá: dùng mô hình **"app cập nhật nhỏ" + "app chính lớn"**: partition `factory` nhỏ (khoảng 0.5 MB, chỉ làm nhiệm vụ tải và ghi firmware) và `ota_0` lớn (khoảng 3 MB). App chính muốn cập nhật thì đặt chế độ khởi động sang `factory` rồi khởi động lại; `factory` tải bản mới, ghi vào `ota_0`, rồi khởi động lại vào `ota_0`. Phức tạp hơn, và nếu mất điện giữa chừng thì app chính hỏng nhưng bản `factory` vẫn cứu được.
- Phương án cuối: giữ `huge_app.csv`, cập nhật qua cáp USB, không OTA.

### Cách thực hiện OTA đề xuất (cho phương án A)

1. **OTA kéo từ Pi 5 (HTTP trong mạng LAN):** Pi 5 chạy web server nhỏ chứa `firmware.bin` và `version.json` (`{"version":"1.2.0","md5":"...","size":N}`). ESP32 kiểm tra định kỳ hoặc khi người dùng bấm nút, dùng `HTTPUpdate`/`Update` để tải và ghi vào vùng còn lại, kiểm tra MD5 rồi khởi động lại. Dùng HTTP thường trong LAN để tránh tốn RAM cho TLS.
2. **ArduinoOTA (espota) cho lúc phát triển:** nạp từ PlatformIO qua mạng (`upload_protocol = espota`, `upload_port = <ip>`), đặt mật khẩu OTA.
3. **Kích hoạt từ MQTT hoặc màn hình:** topic `esp32/ota/cmd` (payload `update`) hoặc nút trong màn Cài đặt (xem đề xuất ở các task trước).
4. **Giao diện khi cập nhật:** màn hình riêng hiển thị `Updating firmware…`, thanh tiến trình %, không cho thao tác khác; cập nhật xong tự khởi động lại. Lỗi thì báo lỗi ngắn rồi quay lại giao diện chính.
5. **An toàn:** đặt mật khẩu cho ArduinoOTA; chỉ chấp nhận nguồn trong LAN; kiểm tra kích thước và MD5 trước khi chuyển vùng khởi động; không cho OTA khi pin hoặc nguồn không ổn định (nếu có pin).
6. **Tự hoàn tác (rollback):** kiểm tra xem bộ khởi động trong Arduino core đang dùng có bật `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` không. Nếu có thì gọi `esp_ota_mark_app_valid_cancel_rollback()` sau khi chạy ổn (đã kết nối Wi-Fi và MQTT); nếu không có thì chấp nhận rủi ro hoặc thêm kiểm tra thủ công.

### Việc cần làm cho phần 3

1. Build với `partitions_ota.csv` và xem kích thước firmware (`pio run` in dung lượng). Báo kích thước thực tế so với 1.94 MB.
2. Nếu vừa: thực hiện OTA phương án A; nếu không: giảm kích thước (phương án B) rồi đo lại.
3. Thêm module `ota.*` (kéo từ Pi 5, kiểm tra MD5, màn hình tiến trình) và hỗ trợ ArduinoOTA khi dev.
4. Viết script/service trên Pi 5 phát `firmware.bin` và `version.json`.
5. Cập nhật `platformio.ini` (partition) và `CLAUDE.md` (cách build và nạp OTA).

### Tiêu chí hoàn thành

- Firmware build vừa trong vùng `ota_0` (kể cả khi `ota_1` dùng cho bản kế tiếp).
- Cập nhật OTA từ Pi 5 thành công từ vùng này sang vùng kia và ngược lại (ít nhất 2 lần liên tiếp).
- Ngắt Wi-Fi hoặc cúp nguồn giữa chừng khi tải: thiết bị vẫn khởi động lại vào bản cũ, không bị hỏng.
- Màn hình hiển thị tiến trình và báo lỗi rõ ràng.

---

## 4. Việc chung

1. Cập nhật `CLAUDE.md` và `task_4.md` phần partition: thay `huge_app.csv` bằng `partitions_ota.csv`, nêu lý do.
2. Chạy `pio run` và kiểm tra không có warning mới; ghi kích thước firmware vào ghi chú.
3. Giữ nguyên quy tắc: chỉ gọi LVGL từ task LVGL; việc mạng chạy ở task riêng.

## 5. Cần xác nhận / thông tin cần cung cấp

- **Log serial** khi gọi API thời tiết (mã `HTTPClient::GET()`, heap còn lại) để chốt nguyên nhân phần 2.
- Đoạn code cập nhật icon Wi-Fi và code gọi API thời tiết (hoặc đường dẫn file) để xác định đúng giả thuyết.
- **Kích thước firmware hiện tại** (dòng cuối của `pio run`).
- OTA có cần cho bản phát hành chính thức, hay chỉ dùng khi phát triển?
