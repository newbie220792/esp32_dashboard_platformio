# task_9.md — Màn 9: System info / Network, và dòng menu "System info" trong Settings

Board: Guition JC4827W543C (480x272, LVGL 9.x, PlatformIO + Arduino). Làm tiếp sau `task_8.md`, giữ nguyên kiến trúc phân tầng ở `task_7.md`. Bản thiết kế nằm trên canvas, màn **7 · Settings** và **9 · System info / Network**.

## 1. Mục tiêu
- Thêm một dòng **System info** vào màn Settings, cùng cấp với Brightness, Screen off, Temperature, Location. Chạm vào thì mở màn 9.
- Màn 9 cho xem nhanh tình trạng mạng và thiết bị ngay trên màn hình, không cần cắm cáp serial. Màn này giúp tìm lỗi ở `task_5.md` (icon Wi-Fi, thời tiết không tải được vì TLS hoặc sai giờ) và theo dõi giới hạn dung lượng OTA.
- **Không cần API hay thay đổi nào ở Pi 5.** Mọi số liệu lấy từ chính ESP32, riêng "Pi 5 API" dùng một yêu cầu nhẹ tới Pi 5 (mục 4).

## 2. Thay đổi ở màn 7 (Settings)
- Settings giờ có **5 hàng**: Brightness, Screen off, Temperature, Location, **System info**. Để vừa màn hình không phải cuộn, mỗi hàng cao **36px** (trước là 44px), khoảng cách **6px**. Các nút bên trong cao 32px nên vẫn vừa. Cập nhật lại các giá trị 44px đã ghi ở `task_8.md`.
- Hàng **System info**: nền `#151B23`, bo 12, padding ngang 12. Nhãn bên trái rộng 96px (font 14 đậm). Bên phải là dòng phụ `Network, memory, firmware` (font 12, màu `#8B98A8`) và mũi tên `›` 18px màu `#E6EDF3`.
- Hàng này là **hàng điều hướng** nên **có thể chạm** (khác Location là chỉ đọc). Vùng chạm là cả hàng. Dòng phụ là chuỗi cố định, không phụ thuộc dữ liệu.
- Chạm vào thì mở màn 9 bằng `lv_screen_load_anim` (trượt sang trái, khoảng 200 ms).

## 3. Màn 9 (System info / Network)

### 3.1 Bố cục
- Là **màn phủ riêng**, giống Alerts và Settings, không nằm trong dải vuốt ngang 4 tab.
- Thanh trên cao 36px: nút `‹ Back` (cao 32px, nền `#151B23`, bo 10) và nhãn `System info`.
- **Nút Back quay về màn 7 (Settings)**, không quay về tab.
- Dưới thanh trên là **hai thẻ cạnh nhau**, mỗi thẻ rộng bằng nhau, khoảng cách 8px, nền `#151B23`, bo 12, padding 8px dọc và 12px ngang. Tiêu đề thẻ cao 18px, font 12 đậm, chữ hoa, màu `#3DD6C6`.
- Không cuộn, không có nút thao tác. Màn chỉ đọc.

### 3.2 Thẻ Network (6 dòng, mỗi dòng cao 26px)
Nhãn bên trái (font 12, màu `#8B98A8`), giá trị bên phải (font 12 đậm).

| Dòng | Giá trị | Nguồn | Ghi chú |
|---|---|---|---|
| Wi-Fi | tên mạng, ví dụ `HomeNet-5G` | `WiFi.SSID()` | Cắt bằng dấu `…` nếu dài. Lọc ký tự ngoài ASCII thành `?` (xem mục 8). Nếu chưa kết nối thì `--`. |
| IP | `192.168.1.57` | `WiFi.localIP()` | Chưa có IP thì `--`. |
| Signal | 4 vạch và `-58 dBm` | `WiFi.RSSI()` | Ngưỡng ở bảng bên dưới. |
| MQTT | chấm và `Connected` | `mqtt.connected()` | Xanh `#3DDC84` khi nối được, đỏ `#FF6B6B` và `Offline` khi mất. |
| Pi 5 API | chấm và `OK · 12 ms` | yêu cầu kiểm tra sức khỏe | Xanh khi trả 200, đỏ và `Error` khi lỗi hoặc quá hạn, xám và `--` khi chưa thử. |
| Time | chấm và `Synced 14:02` | NTP | Xanh khi đã đồng bộ (giờ lần đồng bộ gần nhất), đỏ và `Not synced` khi chưa. |

Ngưỡng tín hiệu Wi-Fi (số vạch sáng trên tổng 4 vạch):

| RSSI | Vạch sáng | Màu vạch sáng |
|---|---|---|
| ≥ -55 dBm | 4 | `#3DDC84` |
| -56 đến -65 | 3 | `#3DDC84` |
| -66 đến -75 | 2 | `#F5A524` |
| -76 đến -85 | 1 | `#FF6B6B` |
| < -85 hoặc mất Wi-Fi | 0 | vạch xám `#3A4656`, giá trị `--` |

Vạch không sáng dùng màu `#3A4656`. Cao 4 vạch lần lượt khoảng 5, 8, 11, 14px, rộng 3px, cách nhau 2px.

### 3.3 Thẻ Device (2 dòng chữ và 3 thanh dung lượng)
Hai dòng chữ cao 26px, ba thanh mỗi thanh cao 36px (một dòng chữ 12px và một thanh dày 6px, bo 3, nền thanh `#263140`).

| Mục | Giá trị mẫu | Nguồn | Màu thanh |
|---|---|---|---|
| Firmware | `v0.8.2` | macro `FW_VERSION` (mục 5) | không có thanh |
| Uptime | `2d 4h 12m` | `esp_timer_get_time()` | không có thanh |
| Heap free | `182 / 320 KB` | `ESP.getFreeHeap()` / `ESP.getHeapSize()` | `#3DD6C6`; vàng `#F5A524` khi còn dưới 20%, đỏ khi còn dưới 10% |
| PSRAM free | `7.1 / 8 MB` | `ESP.getFreePsram()` / `ESP.getPsramSize()` | `#9AA8FF`; vàng khi còn dưới 20%, đỏ khi còn dưới 10% |
| Firmware size | `1.62 / 1.94 MB` | `ESP.getSketchSize()` / kích thước phân vùng app | `#F5A524`; xanh `#3DD6C6` khi dưới 80%, vàng từ 80%, đỏ từ 95% |

- Thanh **Heap** và **PSRAM** thể hiện phần **còn trống**: thanh càng ngắn thì càng nguy hiểm. Thanh **Firmware size** thể hiện phần **đã dùng**: thanh càng dài thì càng gần giới hạn OTA. Cần ghi rõ nhãn để không nhầm chiều.
- Giới hạn Firmware size lấy từ kích thước phân vùng OTA của `partitions_ota.csv` (khoảng 1.94 MB, xem `task_5.md`). Nếu board đang dùng bảng phân vùng không có OTA thì hiển thị `No OTA` thay cho tỷ lệ.
- Màu cảnh báo chỉ đổi màu thanh, không thêm biểu tượng hay hộp thoại.

## 4. Nguồn dữ liệu và làm mới
- Dịch vụ **`sysinfo_service`** (tầng services) gom dữ liệu vào một struct `SysInfo` (chỉ số, chuỗi cố định kích thước, không cấp phát động). Tầng này **không gọi LVGL**.
- Các hàm đo như `WiFi.RSSI()`, `ESP.getFreeHeap()` rất nhẹ, có thể gọi trực tiếp từ tầng hal.
- **Pi 5 API**: gọi `GET /api/health` (Pi 5 trả `200` và `{"ok":true}`), đo thời gian khứ hồi bằng ms. Chỉ gọi khi màn 9 đang mở và Wi-Fi đã có IP, mỗi 10 giây một lần, tối đa 1 yêu cầu chạy cùng lúc, timeout 3 giây, trong task HTTP riêng (không gọi từ task LVGL). Vì Alerts đã có Bearer token, dùng lại cùng cơ chế xác thực. **Đây là điểm duy nhất cần Pi 5 hỗ trợ**: nếu chưa có `/api/health` thì tạm dùng `GET /api/alerts/summary` (từ `task_8.md`) và lấy độ trễ của nó.
- **MQTT** lấy trạng thái từ client hiện có, không mở kết nối mới.
- **Time** lấy từ callback đồng bộ SNTP (`sntp_set_time_sync_notification_cb`), lưu giờ lần đồng bộ gần nhất. Coi là chưa đồng bộ khi `time(nullptr)` nhỏ hơn mốc hợp lý (ví dụ 1.7e9).
- Làm mới màn bằng `lv_timer` **1 giây** khi màn 9 đang hiển thị. Tạo timer khi mở màn, **xóa khi rời màn** để không tốn CPU. Chỉ cập nhật nhãn và thanh khi giá trị đổi (so sánh với lần trước) để tránh vẽ lại không cần thiết.
- Định dạng uptime: `Nd Nh Nm` khi từ 1 ngày trở lên, `Nh Nm` khi dưới 1 ngày, `Nm` khi dưới 1 giờ. Dùng bộ đếm 64-bit, không dùng `millis()` 32-bit (tràn sau khoảng 49 ngày).
- Dung lượng: dưới 1 MB hiển thị `KB` (số nguyên), từ 1 MB hiển thị `MB` với 1 đến 2 chữ số thập phân.

## 5. Phiên bản firmware
- Lấy từ macro `FW_VERSION`, khai báo trong `platformio.ini` bằng `build_flags = -DFW_VERSION=\"0.8.2\"`. Không hard-code trong mã nguồn UI.
- Hiển thị dạng `v` cộng số phiên bản. Nếu macro chưa định nghĩa thì hiển thị `v?` và báo warning khi biên dịch, không để build lỗi.
- Phiên bản này dùng lại cho kiểm tra cập nhật OTA ở `task_5.md` nên nên giữ một nguồn duy nhất.

## 6. Phân tầng (theo `task_7.md`)
- `hal/`: các hàm đọc phần cứng và hệ thống (RSSI, heap, PSRAM, sketch size, thời gian chạy).
- `services/sysinfo_service.*`: gom dữ liệu vào `SysInfo`, gọi `/api/health`, xử lý cờ đồng bộ NTP. Không gọi LVGL.
- `ui/screen_sysinfo.*`: dựng màn 9, đăng ký `lv_timer`, đọc bản sao `SysInfo` để cập nhật. Chỉ tầng `ui/` gọi LVGL.
- `ui/screen_settings.*`: thêm hàng thứ 5 và điều hướng sang màn 9.
- `app/`: đăng ký màn 9 vào bộ điều hướng màn phủ (Back về Settings).
- Giao tiếp giữa task HTTP và UI qua state và sự kiện như các dịch vụ khác. Task HTTP ghi vào bản sao có khóa mutex, UI đọc bản sao đó.
- cập nhật cấu trúc thư mục này vào claude.md cho những session sau này

## 7. Bộ nhớ và hiệu năng
- Màn 9 không có danh sách động: khoảng 40 đối tượng cố định (nhãn, chấm, thanh). Tạo khi mở màn và **hủy khi rời màn** (dùng `auto_del` khi chuyển màn) để không giữ bộ nhớ khi không dùng.
- Đo `lv_mem_monitor` trước khi mở và sau khi rời màn 9: lượng bộ nhớ LVGL phải trở về mức cũ (sai lệch không đáng kể).
- Chuỗi hiển thị dùng bộ đệm tĩnh và `lv_label_set_text_fmt` hoặc `lv_label_set_text_static` khi có thể, tránh cấp phát lặp lại mỗi giây.
- Task HTTP cho `/api/health` có stack khoảng 6 KB (nhẹ hơn Alerts, vì không phân tích JSON lớn).

## 8. Quy tắc hiển thị và an toàn
- **Chỉ dùng ASCII**, không thêm font tiếng Việt (đã chốt ở `task_8.md`). Tên Wi-Fi có thể chứa ký tự lạ nên phải lọc ký tự ngoài ASCII thành `?` trước khi đưa vào nhãn.
- Không hiển thị mật khẩu Wi-Fi, token hay địa chỉ MAC trên màn này.
- Mọi giá trị chưa có hiển thị `--` với chấm xám `#6B7788`, không để trống và không hiển thị `0` gây hiểu nhầm.
- Mất Wi-Fi: màn vẫn mở được, các dòng mạng chuyển về `--` hoặc `Offline`, các dòng Device vẫn cập nhật bình thường.

## 9. Các bước thực hiện
1. Thêm macro `FW_VERSION` vào `platformio.ini` (mục 5).
2. Viết các hàm đọc ở `hal/` và struct `SysInfo`.
3. Viết `sysinfo_service`: gom dữ liệu, gọi `/api/health`, cờ NTP.
4. Dựng `screen_sysinfo` đúng kích thước và màu ở mục 3, đăng ký `lv_timer` 1 giây, hủy khi rời màn.
5. Sửa `screen_settings`: hạ chiều cao hàng xuống 36px, khoảng cách 6px, thêm hàng System info và điều hướng.
6. Nối nút Back của màn 9 về Settings.
7. Kiểm tra mất Wi-Fi, mất MQTT, Pi 5 tắt, chưa đồng bộ NTP.
8. Đo bộ nhớ trước và sau khi mở rồi rời màn 9 nhiều lần.
9. Cập nhật `CLAUDE.md` và `docs/` (màu, ngưỡng, danh sách số liệu).

## 10. Tiêu chí hoàn thành
- Settings có đúng 5 hàng, không cuộn, hàng System info chạm được và mở màn 9; Back về Settings.
- Màn 9 hiển thị đủ 6 dòng Network và 5 mục Device, khớp bố cục và màu trên canvas.
- Số liệu thay đổi theo thực tế: tắt Wi-Fi thì dòng Wi-Fi, IP, Signal, Pi 5 API chuyển trạng thái trong vòng vài giây; dừng broker thì MQTT chuyển đỏ.
- Heap, PSRAM và Firmware size khớp với giá trị in ra qua cổng serial.
- Mở rồi rời màn 9 hàng chục lần không làm tụt heap hay bộ nhớ LVGL (không rò rỉ), timer được xóa khi rời màn.
- Màn không treo khi Pi 5 không phản hồi (timeout 3 giây, không chặn UI).
- `pio run` build thành công, không warning mới.