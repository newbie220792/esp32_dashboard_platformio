---
paths:
  - "src/services/wifi_service.*"
  - "src/ui/screens/screen_wifi.*"
  - "src/core/config.*"
---

## Wi-Fi

Task `wifi` (core 0, `wifi_service_task`) chạy luồng cấu hình cho đến khi kết nối được, sau đó giám sát kết nối lại;
NTP do `time_service` bật khi có Wi-Fi. Mỗi lần đổi trạng thái ghi vào state store (`state_set_wifi_setup`, nhóm
`StateGroup::WifiSetup`); `ui_manager.cpp` hiện screen Wi-Fi (`screen_wifi.cpp`) và chỉ chuyển sang giao diện chính
(tileview) khi trạng thái là `Connected`. Mạng đã lưu đọc/ghi qua `core/config` (`config_*_wifi`).

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
- Giữ BOOT 5 giây (`hal_button_poll` → task `input` trong `app.cpp` → `WifiCmd::ResetAndPortal` + event `WifiReopen`): xóa NVS rồi mở portal, kể cả khi đang ở giao diện chính.
- Sau khi đã vào giao diện chính, mất Wi-Fi chỉ làm icon chuyển xám và tự kết nối lại, không quay về màn cấu hình.
- `WiFi.persistent(false)`: thông tin Wi-Fi chỉ nằm trong NVS của mình.

### Giao diện (480x272)

- Thanh trên 36px: nút `‹ Back` cao 32px (nền `#151B23`, bo 10), nhãn màn, icon Wi-Fi xám bên phải.
  `Back` (`wifi_back()` trong `ui_manager.cpp`) về màn 1 của giao diện chính, luồng cấu hình vẫn chạy nền. Màn Wi-Fi chỉ hiện lại khi
  chạm icon Wi-Fi trên thanh trạng thái, giữ BOOT 5 giây, hoặc kết nối xong rồi lại cần cấu hình.
- Màn 4: icon 44px, nút cao 44px font 16. Màn 5: khung QR 136x136. Màn 6: icon 32px, thẻ thông tin 52px.
  Thẻ trạng thái `Waiting for setup...` cao 36px.
