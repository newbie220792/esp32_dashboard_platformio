# Task: Bỏ khung biểu đồ CPU ở màn 1 (Pi 5 Monitor)

## Bối cảnh

Project ESP32 + LVGL 9 hiển thị thông số Raspberry Pi 5 qua MQTT (màn 1) và thời tiết (màn 2).
Thiết kế tham chiếu: https://claude.ai/artifact/8uVQhRJiXyp4GNxhJ5E6Zd (artboard "1 · Pi 5 Monitor (MQTT)").

## Yêu cầu

Ở màn 1, **xóa toàn bộ khung biểu đồ CPU** nằm dưới 3 đồng hồ cung tròn, gồm:

- Thẻ (card) chứa biểu đồ.
- Tiêu đề `CPU · 60s` và chữ `avg N%`.
- Biểu đồ đường `lv_chart` và dữ liệu lịch sử 60 giây của nó.

Màn 1 chỉ còn: thanh trạng thái phía trên và hàng 3 đồng hồ **CPU / RAM / TEMP**.

## Bố cục mới (320x240, nằm ngang)

- Padding ngoài 8px, khoảng cách giữa các phần 6px, bo góc thẻ 10.
- Thanh trạng thái giữ nguyên (cao 20px): chấm MQTT + "Pi 5 MQTT", 2 chấm chỉ trang, icon Wi-Fi + giờ.
- Hàng 3 thẻ đồng hồ chiếm **toàn bộ phần còn lại** (khoảng 198px cao), mỗi thẻ rộng bằng nhau (flex grow = 1).
- Trong mỗi thẻ: `lv_arc` căn giữa, kích thước khoảng **88x88** (thẻ rộng ~97px nên không vượt quá), nét dày 8–10, cung 270 độ, đầu tròn.
  Số giá trị đặt giữa cung, nhãn đặt dưới cung. Có thể tăng cỡ chữ giá trị lên (Montserrat 24 nếu đã bật trong `lv_conf.h`).
- Nội dung trong thẻ căn giữa theo cả chiều dọc để không bị trống phía trên hoặc dưới.
- Màu giữ nguyên: CPU `#3DD6C6`, RAM `#9AA8FF`, TEMP `#F5A524` (đổi sang đỏ khi > 70°C), nền thẻ `#151B23`, track `#263140`.

## Việc cần làm

1. Tìm trong code UI màn 1 (ví dụ `ui/screens/screen_monitor.*`) đoạn tạo thẻ biểu đồ CPU và xóa nó.
2. Xóa code liên quan: `lv_chart`, series, buffer lịch sử CPU, label `CPU · 60s`, label `avg`, và đoạn cập nhật chúng trong callback nhận MQTT.
3. Nếu `lv_conf.h` bật `LV_USE_CHART` chỉ vì màn này, tắt đi để giảm kích thước firmware (kiểm tra màn 2 không dùng).
4. Chỉnh layout: hàng 3 thẻ đồng hồ co giãn để lấp đầy chiều cao còn lại; tăng kích thước cung theo mô tả trên.
5. Giữ nguyên logic cập nhật 3 đồng hồ từ topic `pi5/stats`.

## Tiêu chí hoàn thành

- Màn 1 không còn biểu đồ, chữ `avg` hay `CPU · 60s`.
- 3 đồng hồ lớn hơn, cân đối, không bị cắt hay chồng chữ.
- Giá trị CPU / RAM / TEMP vẫn cập nhật khi có dữ liệu từ `pi5/stats`; cung TEMP đổi đỏ khi > 70°C.
- Màn 2 (thời tiết) và icon Wi-Fi không bị ảnh hưởng.
- `pio run` build thành công, không có warning mới về biến hoặc hàm không dùng.
