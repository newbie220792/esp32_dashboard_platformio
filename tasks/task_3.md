# Task 3: Cấu hình Wi-Fi bằng WiFiManager (ưu tiên `secrets.h`, lặp lại đến khi thành công)

Thiết kế tham chiếu: https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd
(artboard "4 · Wi-Fi: saved network failed", "5 · Wi-Fi: setup mode", "6 · Wi-Fi: setup failed, retry").
Màn hình 320x240 nằm ngang, nền `#0B0F14`, thẻ `#151B23`, font Montserrat 12/14/20.

## 1. Luồng hoạt động

1. **Khởi động:** thử kết nối bằng SSID và mật khẩu trong `include/secrets.h` (ưu tiên số 1). Timeout 15–20 giây.
2. **Thành công:** vào giao diện chính (3 màn vuốt ngang), lưu cấu hình đã dùng được vào NVS.
3. **Thất bại với `secrets.h`:** hiện **màn 4** (báo lỗi + lý do). Đếm ngược 5 giây rồi tự mở chế độ cấu hình. Người dùng có thể bấm **Setup now** để vào ngay, hoặc **Retry** để thử lại `secrets.h`.
4. **Chế độ cấu hình:** ESP32 phát mạng riêng (AP) `ESP32-Setup` kèm trang cấu hình (captive portal của WiFiManager). Hiện **màn 5** hướng dẫn: tên mạng, địa chỉ `192.168.4.1`, mã QR để nối vào mạng.
5. **Người dùng gửi SSID và mật khẩu mới qua trang cấu hình:** ESP32 thử kết nối bằng thông tin mới (hiện trạng thái "Connecting…").
6. **Thành công:** lưu vào NVS, tắt AP, vào giao diện chính.
7. **Thất bại:** hiện **màn 6** (báo lỗi, tên mạng, lý do), **giữ nguyên AP và trang cấu hình mở**, cho phép nhập lại. Lặp lại bước 5 cho đến khi kết nối thành công. Không giới hạn số lần thử, không tự khởi động lại.

Những lần khởi động sau: thử thông tin đã lưu từ NVS trước, rồi mới đến `secrets.h`. Nếu bạn muốn `secrets.h` luôn thắng thì bỏ NVS khỏi bước này (xem mục 5).

## 2. Bố cục các màn hình

### Màn 4: Kết nối bằng `secrets.h` thất bại

- Thanh trên: nhãn `Wi-Fi`, icon Wi-Fi xám `#6B7788` ở bên phải.
- Giữa màn hình, căn giữa: icon Wi-Fi gạch chéo đỏ `#FF6B6B` 40px, tiêu đề `Can't connect` (font 20 đậm), dòng `Network: <SSID>` (12, `#8B98A8`), dòng lý do (12 đậm, `#FF6B6B`).
- Lý do lấy từ trạng thái Wi-Fi: `Network not found` (`WL_NO_SSID_AVAIL`), `Wrong password` hoặc `Connection failed` (`WL_CONNECT_FAILED`), `Timeout`.
- Dưới: nhãn `Setup starts in Ns` và thanh tiến trình 4px (track `#263140`, phần chạy `#3DD6C6`).
- Dưới cùng: 2 nút cao 36px, bo góc 10, chia đôi hàng:
    - `Retry` (nền `#2A3646`, chữ `#E6EDF3`): thử lại kết nối.
    - `Setup now` (nền `#3DD6C6`, chữ `#0B0F14`): vào chế độ cấu hình ngay.

### Màn 5: Chế độ cấu hình đang mở

- Thanh trên: nhãn `Wi-Fi setup`, icon Wi-Fi xám.
- Trái: khung mã QR 104x104 (dùng `lv_qrcode`), nội dung chuỗi nối mạng `WIFI:T:nopass;S:ESP32-Setup;;`. Nếu AP có mật khẩu thì đổi `T:WPA;P:<mật khẩu>`.
- Phải: `Join this network` (12, muted) / `ESP32-Setup` (14 đậm, `#3DD6C6`) / `Then open` / `192.168.4.1` (14 đậm, `#3DD6C6`) / `The page may open automatically.`
- Dưới: thẻ 28px có chấm vàng `#F5A524` và chữ `Waiting for setup…`.

### Màn 6: Cấu hình mới thất bại, thử lại

- Thanh trên giống màn 5.
- Giữa: icon Wi-Fi gạch chéo đỏ 32px, tiêu đề `Couldn't connect`, dòng `Network: <SSID vừa nhập>`, dòng lý do đỏ (`Wrong password` v.v.).
- Thẻ thông tin 50px: dòng 1 `Setup is open again` (trái) và `Attempt N` (phải, N tăng dần); dòng 2 `ESP32-Setup · 192.168.4.1` (`#3DD6C6`, 12 đậm).
- Dưới: thẻ 28px chấm vàng + `Waiting for setup…`.
- Màn này giữ AP và trang cấu hình mở, không có nút nào ngoài việc chờ người dùng gửi lại.

Cần thêm một trạng thái "Connecting…" ngắn (spinner `lv_spinner` kèm tên mạng) khi đang thử kết nối ở bước 1 và bước 5. Chưa thiết kế, dùng cùng phong cách màn 4.

## 3. Việc cần làm

1. Thêm thư viện `tzapu/WiFiManager` vào `platformio.ini`.
2. Tạo module `services/wifi_service.*` quản lý trạng thái: `CONNECTING`, `FAILED_SAVED`, `PORTAL`, `CONNECTING_NEW`, `FAILED_NEW`, `CONNECTED`.
3. Thử `secrets.h` bằng `WiFi.begin()` với timeout 15–20 giây, đọc `WiFi.status()` để biết lý do lỗi.
4. Chế độ cấu hình: dùng `WiFiManager` ở chế độ không chặn (`setConfigPortalBlocking(false)`) và `startConfigPortal("ESP32-Setup")`, để vòng lặp LVGL vẫn chạy và cập nhật màn hình.
5. Dùng `setSaveConfigCallback` / theo dõi `WiFi.status()` để biết khi nào người dùng gửi thông tin và đang thử kết nối, từ đó chuyển trạng thái và tăng số lần thử.
6. Khi thất bại ở bước 5 phải mở lại portal (hoặc giữ portal chạy) để nhập lại, không thoát khỏi chế độ cấu hình.
7. Dựng 3 màn hình mới `ui_wifi_fail.*`, `ui_wifi_setup.*`, `ui_wifi_retry.*` (hoặc một file `ui/screens/screen_wifi.*` có 3 chế độ). Hiển thị chúng trên một màn riêng, ẩn tileview chính cho đến khi kết nối thành công.
8. Chỉ gọi LVGL từ task LVGL; sự kiện Wi-Fi gửi qua hàng đợi (queue) cho task giao diện.
9. Cập nhật `CLAUDE.md`: thêm mục Wi-Fi và luồng ở trên.

## 4. Tiêu chí hoàn thành

- Với `secrets.h` đúng: vào thẳng giao diện chính, không qua màn cấu hình.
- Với `secrets.h` sai: hiện màn 4 kèm lý do đúng, đếm ngược 5 giây rồi mở portal, nút `Retry` và `Setup now` hoạt động.
- Trong portal, nối điện thoại vào `ESP32-Setup`, trang cấu hình mở được ở `192.168.4.1`; quét QR trên màn 5 nối được mạng.
- Nhập sai mật khẩu: hiện màn 6, `Attempt` tăng, portal vẫn mở và nhập lại được; lặp lại không giới hạn.
- Nhập đúng: vào giao diện chính, lưu cấu hình, khởi động lại vẫn kết nối được mà không cần cấu hình lại.
- Giao diện không bị treo trong lúc kết nối hoặc chờ portal.
- `pio run` build thành công, không có warning mới.

## 5. Cần xác nhận

- **Thứ tự ưu tiên sau khi đã cấu hình qua portal:** `secrets.h` luôn thắng, hay thông tin NVS (do người dùng nhập) thắng? Nếu `secrets.h` luôn thắng thì cấu hình mới qua portal sẽ bị bỏ qua ở lần khởi động sau khi `secrets.h` đúng, hãy chốt rõ.
- AP `ESP32-Setup` có đặt mật khẩu không (nên có để tránh người lạ vào cấu hình). Nếu có thì mật khẩu hiển thị trên màn 5.
- Có cho phép giữ nút BOOT để xóa cấu hình đã lưu và buộc vào portal không.
