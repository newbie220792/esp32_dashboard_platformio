# Task 2: Danh sách service (màn 1) và điều khiển công tắc nhà (màn 3)

Thay thế và mở rộng `task.md`. Thiết kế tham chiếu (artboard "1 · Pi 5 Monitor", "3 · Home switches"):
https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd

Màn hình 320x240 nằm ngang, 3 tile vuốt ngang (`lv_tileview`): 1 Pi 5 Monitor, 2 Weather, 3 Home switches.
Màn 3 điều khiển bằng chạm, nên màn hình **cần có cảm ứng**.

## 0. Thay đổi chung

- Thanh chỉ trang ở thanh trạng thái đổi từ 2 thành **3 chấm** (chấm đang chọn dài 16x4 `#3DD6C6`, còn lại 6x4 `#3A4656`).
- Màn 2 (Weather) không đổi gì ngoài 3 chấm chỉ trang.
- Icon Wi-Fi giữ nguyên: xanh `#3DDC84` khi kết nối, xám `#6B7788` khi mất kết nối, ở cả 3 màn.

## 1. Màn 1: Pi 5 Monitor

### 1.1 Bố cục mới

Từ trên xuống (padding 8px, khoảng cách 6px):

1. Thanh trạng thái 20px (không đổi).
2. Hàng 3 đồng hồ CPU / RAM / TEMP, **cao 80px**, thu nhỏ so với trước:
    - `lv_arc` 56x56, nét 6, cung 270 độ, đầu tròn.
    - Giá trị ở giữa cung, font 14: `37%`, `52%`, `58°`. Nhãn dưới cung font 12: `CPU`, `RAM 4.1/8G`, `TEMP`.
    - Màu giữ nguyên: CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524` (đỏ khi > 70°C).
3. **Danh sách service** chiếm phần còn lại (khoảng 112px), thẻ nền `#151B23`, bo góc 10, padding 6x10.

### 1.2 Khung biểu đồ CPU đã bỏ

Xóa hoàn toàn khung biểu đồ CPU (card, `lv_chart`, buffer lịch sử, label `CPU · 60s` và `avg`). Danh sách service thay vào chỗ đó. Nếu `LV_USE_CHART` không còn dùng ở đâu thì tắt trong `lv_conf.h`.

### 1.3 Danh sách service

- Hàng tiêu đề (font 12, màu `#8B98A8`): `Service` | `Status` | `Port`.
- Mỗi dòng cao 20px, font 12, gồm 3 cột:
    - **Service**: tên, đậm (600), co giãn, cắt bằng `...` nếu dài.
    - **Status**: rộng 48px, căn giữa, **chỉ có chấm tròn 8px, không có chữ**.
        - `running`: chấm đặc xanh `#3DDC84`.
        - `stopped`: vòng tròn rỗng viền 2px đỏ `#FF6B6B` (khác hình để người khó phân biệt màu vẫn nhận ra).
        - Gắn label truy cập (accessibility) nếu framework hỗ trợ.
    - **Port**: rộng 44px, căn phải, màu `#8B98A8`; không có port thì hiện `-`.
- Hiển thị vừa 4 dòng. Nhiều hơn thì **cuộn dọc** (`lv_obj` scrollable hoặc `lv_table`), thanh cuộn mảnh 3px màu `#3A4656` ở mép phải.
- Dữ liệu danh sách do Pi 5 publish, ESP32 chỉ parse và vẽ, **không hard-code** danh sách service.

### 1.4 Dữ liệu MQTT cho service

Topic `pi5/services`, Pi 5 publish khi có thay đổi và lặp lại định kỳ (ví dụ 10 giây), nên đặt retain:

```json
{
    "services": [
        { "name": "nginx", "status": "running", "port": 80 },
        { "name": "mosquitto", "status": "running", "port": 1883 },
        { "name": "docker", "status": "running", "port": null },
        { "name": "node-red", "status": "stopped", "port": 1880 }
    ]
}
```

- `status` chỉ nhận `running` hoặc `stopped`; giá trị lạ coi như `stopped`.
- `port` có thể `null`.
- Tăng buffer `PubSubClient::setBufferSize(2048)` (mặc định 256 byte sẽ cắt gói tin).
- Parse bằng ArduinoJson, giới hạn tối đa khoảng 10 service để tiết kiệm RAM. Nếu JSON hỏng thì giữ nguyên danh sách cũ, không xóa.
- Topic `pi5/stats` giữ nguyên như cũ.

## 2. Màn 3: Home switches (màn mới)

### 2.1 Phân cấp 3 lớp

1. **Panel phòng** (lớp ngoài): mỗi phòng là một panel cha.
2. **Panel cụm** (lớp giữa): trong phòng có nhiều cụm công tắc, mỗi cụm là một panel.
3. **Công tắc** (lớp trong): mỗi cụm có **đúng 3 công tắc** xếp ngang.

### 2.2 Bố cục

- Thanh trạng thái 20px: chấm MQTT + nhãn `Home MQTT`, 3 chấm chỉ trang, icon Wi-Fi + giờ.
- **Panel phòng**: thẻ nền `#151B23`, bo góc 10, padding 6, chiếm toàn bộ phần còn lại (khoảng 198px).
    - Hàng tiêu đề cao 24px: nút `‹` (rộng 32px, trái), tên phòng font 14 đậm kèm số thứ tự `1/4` font 12 màu `#8B98A8` (giữa), nút `›` (rộng 32px, phải). Chạm `‹` `›` để đổi phòng.
    - Phía dưới là danh sách panel cụm, xếp dọc, khoảng cách 4px.
- **Panel cụm**: nền `#1D2733`, bo góc 8, padding 4. Có nhãn cụm (ví dụ `Panel A`) font 12 màu `#8B98A8` cao 14px, bên dưới là hàng 3 công tắc.
- **Công tắc**: bo góc 8, khoảng cách ngang 6px, chiều rộng chia đều 3 phần. Nội dung căn giữa: icon 20px, tên font 12 đậm, trạng thái `ON` / `OFF` font 12.
    - ON: nền `#F5A524`, chữ và icon `#0B0F14`.
    - OFF: nền `#2A3646`, tên `#E6EDF3`, trạng thái và icon `#A3AEBB`.
- Màn hiển thị vừa **2 cụm**. Phòng có nhiều cụm hơn thì **cuộn dọc** trong panel phòng (hàng tiêu đề đứng yên).
- Icon dùng ký hiệu LVGL có sẵn hoặc bitmap nhỏ: bóng đèn, quạt, ổ cắm, điều hòa. Chọn icon theo trường `icon` trong dữ liệu.

### 2.3 Dữ liệu MQTT

Cấu trúc phòng, cụm và công tắc do broker hoặc Pi 5 publish, ESP32 dựng giao diện từ đó (không hard-code phòng).

Topic cấu hình `home/config` (retain):

```json
{
    "rooms": [
        {
            "id": "living",
            "name": "Living room",
            "panels": [
                {
                    "id": "a",
                    "name": "Panel A",
                    "switches": [
                        {
                            "id": 1,
                            "name": "Ceiling",
                            "icon": "bulb",
                            "state": "ON"
                        },
                        {
                            "id": 2,
                            "name": "Wall",
                            "icon": "bulb",
                            "state": "OFF"
                        },
                        { "id": 3, "name": "Fan", "icon": "fan", "state": "ON" }
                    ]
                },
                {
                    "id": "b",
                    "name": "Panel B",
                    "switches": [
                        {
                            "id": 1,
                            "name": "AC",
                            "icon": "snow",
                            "state": "OFF"
                        },
                        {
                            "id": 2,
                            "name": "TV",
                            "icon": "plug",
                            "state": "OFF"
                        },
                        {
                            "id": 3,
                            "name": "Lamp",
                            "icon": "bulb",
                            "state": "ON"
                        }
                    ]
                }
            ]
        }
    ]
}
```

- Cập nhật trạng thái từng công tắc: topic `home/<phòng>/<cụm>/<số>/state`, payload `ON` hoặc `OFF`.
- **Gửi lệnh**: khi chạm công tắc, publish `ON` hoặc `OFF` (ngược với trạng thái hiện tại) lên `home/<phòng>/<cụm>/<số>/set`.
- **Chỉ đổi màu ô khi nhận được phản hồi ở `.../state`**, không đổi ngay lúc chạm (tránh hiện ON khi thiết bị chưa bật). Có thể làm mờ ô trong lúc chờ phản hồi và hết thời gian chờ (ví dụ 3 giây) thì trả về trạng thái cũ.
- Buffer MQTT cần đủ lớn cho `home/config` (`setBufferSize(4096)` hoặc hơn, tùy số phòng).
- Chỉ gọi LVGL từ task LVGL; callback MQTT đẩy dữ liệu qua hàng đợi (queue) cho task giao diện.

## 3. Việc cần làm

1. Cập nhật `lv_tileview`: thêm tile thứ 3, đổi chỉ trang thành 3 chấm trên cả 3 màn.
2. Màn 1: bỏ khung biểu đồ, thu nhỏ đồng hồ, thêm danh sách service và parse `pi5/services`.
3. Màn 3: dựng panel phòng, panel cụm, công tắc; đổi phòng bằng `‹` `›`; parse `home/config`; nhận `.../state`; gửi `.../set`.
4. Tách file: `ui/screens/screen_monitor.*`, `ui/screens/screen_weather.*`, `ui/screens/screen_home.*`, `mqtt.*` (nhận chung và phân phối theo topic).
5. Viết script Python cho Pi 5 (psutil + paho-mqtt) publish `pi5/stats` và `pi5/services` (lấy danh sách service bằng `systemctl` và `ss -ltnp`). Nếu bạn đã có script thì chỉ cần sửa theo format trên.
6. Cập nhật `CLAUDE.md`: cấu trúc thư mục, danh sách topic, quy ước giao diện của màn 3.

## 4. Tiêu chí hoàn thành

- Màn 1 không còn biểu đồ; hiển thị 3 đồng hồ nhỏ và danh sách service cuộn được; cột Status chỉ có chấm, không có chữ.
- Danh sách service thay đổi theo gói `pi5/services` mà không cần sửa firmware.
- Màn 3 hiển thị đúng phân cấp phòng, cụm (mỗi cụm 3 công tắc); đổi phòng bằng `‹` `›`; cuộn được khi nhiều cụm.
- Chạm công tắc gửi đúng lệnh `.../set`; ô chỉ đổi màu khi nhận được `.../state`.
- Mất MQTT hoặc Wi-Fi: chấm và icon chuyển xám, các công tắc không gửi lệnh được (báo trạng thái chờ, không treo giao diện).
- `pio run` build thành công, không có warning mới.

## 5. Cần xác nhận

- Hệ thống công tắc đang dùng (Home Assistant, Tasmota, Shelly hay tự làm bằng ESP) để chốt topic và payload thật. Nội dung topic trong file này chỉ là đề xuất.
- Số phòng, tên phòng và số cụm mỗi phòng.
- Màn hình có cảm ứng không, và chip cảm ứng là gì (XPT2046, FT6236...).
