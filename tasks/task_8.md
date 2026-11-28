# Task 8: Màn Settings, màn Alerts (lấy từ Pi 5 qua REST) và lối vào từ thanh trạng thái

Board JC4827W543C, màn 480x272 (xem `task_4.md`). Kiến trúc phân tầng theo `task_7.md`. Thiết kế tham chiếu (artboard "7 · Settings", "8 · Alerts" và thanh trạng thái mới ở màn 1, 2, 3): https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd

> File này viết khi chưa xem code hiện tại. Các giá trị cụ thể (kích thước, chu kỳ gọi API) là đề xuất, cần đo và chốt khi làm thật.

**Thay đổi so với bản trước của file này:**
- Settings: **bỏ hàng Wi-Fi và nút Reset Wi-Fi**; hàng City thành **Location chỉ đọc**, hiển thị vị trí của Pi 5 (mặc định `TP Dong Nai`), không cho sửa.
- Alerts: **tối đa 10 cảnh báo**, Pi 5 trả về một lần; **bỏ phân trang và tải thêm khi cuộn** (không còn `before_id`, `Loading more…`, `10 of 47`). Các phần còn lại giữ như cũ.

## 1. Lối vào từ thanh trạng thái (màn 1, 2, 3)
- Thêm 2 icon ở bên phải thanh trạng thái, trước icon Wi-Fi và giờ: **chuông** (mở Alerts) và **bánh răng** (mở Settings). Icon 20px, khung 28px.
- Tăng vùng chạm bằng `lv_obj_set_ext_click_area` (khoảng 36–40px).
- Chuông có **chấm đỏ** `#FF6B6B` (10px, viền `#0B0F14`) khi số cảnh báo chưa xử lý > 0; ẩn chấm khi bằng 0. Số này lấy từ API tóm tắt (mục 3.2).
- Alerts và Settings là **màn phủ riêng** (`lv_screen_load`), không nằm trong dải vuốt ngang; vuốt ngang vẫn chỉ có 4 tab. Nút `‹ Back` quay về **tab đã rời đi** (lưu chỉ số tab hiện tại trước khi mở).
- Các màn Wi-Fi (4, 5, 6) giữ nút Back về màn 1 như `task_3.md`.
- Nhãn bên trái thanh trạng thái của tab Weather hiển thị tên thành phố lấy từ Pi 5 (`TP Dong Nai`), thay cho `Ho Chi Minh` cũ.

## 2. Màn Settings (artboard 7)
Thanh trên cao 36px: nút `‹ Back` + nhãn `Settings`. Dưới là **4 hàng**, mỗi hàng cao **44px**, nền `#151B23`, bo 12, padding ngang 12, khoảng cách 8px. Nhãn bên trái rộng 96px (font 14 đậm).

| Hàng | Điều khiển | Lưu vào NVS | Hành vi |
|---|---|---|---|
| Brightness | thanh trượt 0–100% (mặc định 70), hiện `%` | `bright` | Đặt PWM đèn nền ngay khi kéo; lưu khi thả. Giới hạn tối thiểu 10% để màn không tắt hẳn. |
| Screen off | nhóm chọn `15s / 30s / 1m / Never` | `sleep_s` | Hết thời gian không chạm thì giảm sáng rồi tắt đèn nền (dùng thời gian không hoạt động của LVGL). Chạm đầu tiên chỉ đánh thức, không kích hoạt nút bên dưới. |
| Temperature | nhóm chọn `°C / °F` | `unit` | Áp dụng cho nhiệt độ thời tiết và nhiệt độ Pi 5 ở mọi màn. Dữ liệu gốc luôn giữ °C, chỉ đổi khi hiển thị. |
| Location | **chỉ đọc**: nhãn `from Pi 5`, tên thành phố, icon khóa | (chỉ lưu bản sao gần nhất) | Không chạm được, không có mũi tên. Xem 2.1. |

- Nút chọn cao 32px, mục đang chọn nền `#3DD6C6` chữ tối, các mục khác nền `#2A3646`.
- Cài đặt đọc từ NVS khi khởi động và áp dụng trước khi vẽ giao diện; thay đổi lưu ngay (hoặc khi thả), không cần nút Save.
- **Đã bỏ khỏi màn này:** OTA, Reset Wi-Fi, chọn thành phố.

### 2.1 Hàng Location (chỉ đọc)
- Hiển thị thành phố hiện tại của Pi 5, mặc định **`TP Dong Nai`**. Chữ màu `#8B98A8` (tối hơn các hàng khác), kèm icon khóa 18px; đặt `LV_STATE_DISABLED` và bỏ cờ clickable, nên người dùng không thể sửa.
- Nguồn dữ liệu: Pi 5 cung cấp qua REST, ví dụ `GET /api/location` trả `{"city":"TP Dong Nai","lat":<số>,"lon":<số>}`. **Tọa độ do Pi 5 cấu hình và trả về, không hard-code trong firmware.** Thời tiết (Open-Meteo) dùng `lat`/`lon` này.
- Đọc khi khởi động và sau mỗi lần có lại mạng; nếu không gọi được thì dùng bản sao lần trước trong NVS, và nếu chưa có thì hiển thị `TP Dong Nai` làm mặc định (tọa độ lấy từ cấu hình mặc định, cần bạn cung cấp).
- Khi vị trí đổi (ở phía Pi 5), cập nhật nhãn ở Settings và ở thanh trạng thái tab Weather, rồi tải lại thời tiết.
- **Font: chỉ dùng ASCII, không dấu.** Mọi chuỗi trên UI (kể cả tên thành phố từ Pi 5) dùng font Montserrat dựng sẵn của LVGL, không tạo font tiếng Việt. Pi 5 nên trả `city` không dấu; firmware vẫn lọc ký tự ngoài ASCII (thay bằng `?`) để không hiện ô vuông.

## 3. Màn Alerts (artboard 8) và API REST từ Pi 5

### 3.1 Giao diện
- Thanh trên: `‹ Back`, nhãn `Alerts`, bên phải số lượng (`4 alerts`) và nút `Clear all` (cao 32px).
- Danh sách cuộn dọc, **tối đa 10 dòng**, mỗi dòng cao 46px, nền `#151B23`, bo 12, khoảng cách 6px, gồm: icon 28px, tiêu đề (14 đậm, cắt `…`), chi tiết (12, `#8B98A8`), thời gian tương đối bên phải (`2 min`). Màn hiển thị vừa 4 dòng, nhiều hơn thì cuộn; thanh cuộn mảnh 3px `#3A4656` ở mép phải.
- **Ba mức độ khác nhau cả về hình lẫn màu** (không chỉ dựa vào màu):
  - Nghiêm trọng: hình tròn đặc đỏ `#FF6B6B` có dấu `!`.
  - Cảnh báo: tam giác đặc vàng `#F5A524` có dấu `!`.
  - Thông tin: hình vuông bo góc viền tím `#9AA8FF` có chữ `i`.
- Trạng thái đang tải lần đầu: vòng quay nhỏ giữa danh sách; rỗng: `No alerts`; lỗi mạng: `Couldn't load alerts` kèm nút `Retry`, vẫn giữ các dòng đã có (chưa thiết kế, dùng cùng phong cách).

### 3.2 API REST (Pi 5, trong mạng LAN)
Dịch vụ nhỏ trên Pi 5 (ví dụ FastAPI hoặc Flask), lưu cảnh báo trong SQLite. Thời gian dùng epoch giây (UTC); ESP32 đổi sang giờ địa phương bằng NTP đã có.

| Phương thức | Đường dẫn | Dùng để |
|---|---|---|
| GET | `/api/alerts/summary` | Số cảnh báo chưa xử lý cho chấm đỏ của chuông. Gọi định kỳ 15–30 giây. |
| GET | `/api/alerts` | Danh sách cảnh báo **tối đa 10 dòng, mới nhất trước**, trả về một lần. |
| POST | `/api/alerts/ack-all` | Nút `Clear all`: đánh dấu mọi cảnh báo đã xử lý. |
| GET | `/api/location` | Thành phố và tọa độ của Pi 5 (mục 2.1). |

`GET /api/alerts/summary`:
```json
{"unacked": 3, "critical": 1, "latest_id": 128}
```

`GET /api/alerts`:
```json
{
  "items": [
    {"id": 128, "level": "critical", "title": "Pi 5 temperature high", "detail": "74°C · limit 70°C", "ts": 1764000000, "count": 3, "ack": false},
    {"id": 127, "level": "warning",  "title": "node-red stopped",      "detail": "Port 1880",           "ts": 1763999400, "count": 1, "ack": false}
  ]
}
```
- **Pi 5 chịu trách nhiệm giới hạn: không bao giờ trả quá 10 dòng**, kể cả khi có nhiều hơn trong cơ sở dữ liệu. ESP32 vẫn kiểm tra và bỏ phần vượt quá 10.
- `level` chỉ nhận `critical`, `warning`, `info`; giá trị lạ coi như `info`.
- **Gộp cảnh báo trùng ở phía Pi 5**: cùng một nguồn lặp lại thì tăng `count` và cập nhật `ts`, không thêm dòng mới.
- `title` ≤ 40 ký tự, `detail` ≤ 60 ký tự (cắt ở phía Pi 5). Một phản hồi tối đa vài KB.
- Mã trạng thái chuẩn: 200 thành công, 400 tham số sai, 401 sai token, 500 lỗi server. Nội dung lỗi: `{"error":"..."}`.
- **Xác thực:** header `Authorization: Bearer <token>`; token lưu trong `secrets.h` (không commit), cấu hình phía Pi 5 qua biến môi trường. Dùng HTTP thường trong LAN (HTTPS tốn RAM trên ESP32), nên: chỉ lắng nghe trên địa chỉ LAN, chặn truy cập từ ngoài bằng tường lửa, và hiểu rằng token đi dạng văn bản thường. API ghi (`POST`) bắt buộc có token; API đọc cũng nên có.

### 3.3 Bộ nhớ và làm mới
- ESP32 chỉ giữ **mảng cố định 10 cảnh báo** (struct kích thước cố định). Widget tối đa 10 dòng; rời màn Alerts thì xóa widget (`lv_obj_clean`), giữ dữ liệu.
- Mở màn: gọi `GET /api/alerts` một lần. Khi màn đang mở, làm mới mỗi 15–30 giây hoặc khi `latest_id` ở `summary` thay đổi. Không gọi chồng khi một yêu cầu đang chạy.
- Dùng `lv_mem_monitor()` và `heap_caps_get_free_size()` để đo RAM trước và sau khi mở màn với 10 mục.

### 3.4 Gọi REST trên ESP32
- Dịch vụ `alerts_service` (tầng services), chạy trong task riêng (stack khoảng 8 KB), **không gọi từ task LVGL**. Kết quả ghi vào state và bắn sự kiện, UI cập nhật qua `lv_timer`.
- `HTTPClient` với timeout 5 giây, tái sử dụng kết nối (`setReuse(true)`); phân tích bằng ArduinoJson với `filter` chỉ lấy các trường cần, vùng nhớ khoảng 4 KB.
- Lỗi mạng/HTTP: giữ dữ liệu cũ, hiện `Couldn't load alerts` + `Retry`; thử lại tự động với lùi dần cho phần tóm tắt (30 s, 60 s, tối đa 5 phút).
- Chỉ gọi API khi Wi-Fi đã có IP; mất Wi-Fi thì dừng và chấm đỏ giữ trạng thái cuối cùng (không nhảy về 0).
- `Clear all`: gửi `POST /api/alerts/ack-all`, chờ 200 rồi mới làm trống danh sách và ẩn chấm đỏ; lỗi thì giữ nguyên và báo lỗi ngắn.

## 4. Nguồn cảnh báo (phía Pi 5)
Pi 5 chạy bộ quy tắc và ghi cảnh báo vào SQLite. Đề xuất ban đầu (ngưỡng đặt trong cấu hình):
- `critical`: nhiệt độ Pi 5 > 70°C; service quan trọng dừng.
- `warning`: RAM > 90%; service không quan trọng dừng; mất kết nối với broker MQTT.
- `info`: sự kiện thông tin khác do Pi 5 tự định nghĩa (không có cảnh báo mưa; thời tiết chỉ hiển thị ở tab Weather).
Cảnh báo tự **đóng** (đánh dấu hết hiệu lực) khi điều kiện hết, để số "chưa xử lý" phản ánh đúng thực tế.

## 5. Phân tầng (theo `task_7.md`)
- `services/alerts_service.*`: gọi REST, mảng 10 mục, summary; không gọi LVGL.
- `services/location_service.*` (hoặc gộp trong `weather_service`): đọc `/api/location`, cập nhật state và NVS.
- `services/settings_service.*` (hoặc trong `core/config`): đọc/ghi NVS, áp dụng độ sáng, thời gian tắt màn hình, đơn vị.
- `core/state`: `alerts_state`, `settings_state`, `location_state`.
- `ui/screens/screen_alerts.*`, `screen_settings.*`; component `status_bar` thêm chuông và bánh răng.
- Phía Pi 5: `tools/alerts_api.py` (REST gồm alerts và location) và bộ quy tắc sinh cảnh báo.

## 6. Việc cần làm
1. Viết dịch vụ REST trên Pi 5 (SQLite, các endpoint ở 3.2, token, gộp trùng, giới hạn 10 dòng, `/api/location` đọc từ cấu hình); thử bằng `curl` trước khi nối ESP32.
2. Viết bộ quy tắc sinh cảnh báo từ dữ liệu Pi 5 (nhiệt độ, RAM, service).
3. `alerts_service` trên ESP32: tóm tắt định kỳ, tải danh sách, mảng 10 mục, xử lý lỗi và thử lại.
4. `screen_alerts`: danh sách cuộn, trạng thái đang tải/rỗng/lỗi, `Clear all`; xóa widget khi rời màn.
5. `screen_settings` và `settings_service`: 4 hàng, lưu NVS, áp dụng ngay; hàng Location chỉ đọc, đọc từ `location_service`.
6. Thêm chuông (có chấm đỏ) và bánh răng vào `status_bar`; mở màn phủ, Back quay về tab trước. Nhãn tab Weather lấy tên thành phố từ `location_state`.
7. Lọc chuỗi về ASCII trước khi đưa vào label (city, tiêu đề cảnh báo).
8. Đo RAM/flash sau khi thêm; kiểm tra firmware còn trong vùng OTA (xem `task_5.md`).
9. Cập nhật `CLAUDE.md` và `docs/` (API, màu và hình biểu tượng mức độ).

## 7. Tiêu chí hoàn thành
- Settings chỉ có 4 hàng; không còn Reset Wi-Fi; Location hiển thị `TP Dong Nai`, không chạm hay sửa được.
- Đổi vị trí ở Pi 5 thì Location và nhãn tab Weather cập nhật theo, thời tiết tải lại.
- Alerts không bao giờ hiển thị quá 10 dòng; cuộn mượt; `Clear all` hoạt động; chấm đỏ khớp với số chưa xử lý.
- Mất mạng/Pi 5: màn không treo, giữ dữ liệu cũ và báo lỗi ngắn.
- `pio run` build thành công, không warning mới; firmware còn trong vùng OTA.

## 8. Đã chốt và còn cần xác nhận
- Đã chốt: chỉ ASCII, không font tiếng Việt; không có cảnh báo mưa; chỉ có `Clear all`, không xử lý từng cảnh báo (không vuốt, không nút ✕).
- **Tọa độ `lat`/`lon` của `TP Dong Nai`:** do Pi 5 cấu hình và trả về; cần bạn xác định điểm dùng cho thời tiết.