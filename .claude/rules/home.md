---
paths:
  - "src/ui/screens/screen_home.*"
  - "src/ui/components/switch_tile.*"
  - "src/services/home_service.*"
  - "tools/home_sim.py"
---

## Màn 3: Home switches

- Panel phòng `#151B23` (bo 12, padding 6). Hàng tiêu đề 32px: nút `‹` `›` 44x32 (nền `#1D2733`),
  tên phòng font 20 + `1/4` (12, `#8B98A8`).
  Đổi phòng vòng tròn. Nhận `home/config` mới thì giữ phòng đang xem nếu id còn tồn tại.
- Panel cụm `#1D2733` (bo 10, padding 4, cao 82px, cách nhau 8px): nhãn cụm font 12 + hàng 3 công tắc.
  Vừa 2 cụm, nhiều hơn thì cuộn (tiêu đề đứng yên).
- Mỗi cụm luôn có đúng 3 ô: thiếu thì để ô trống, thừa thì bỏ.
- Công tắc (bo 10, khoảng 140x58, icon 22px, tên font 14, trạng thái font 12): ON nền `#F5A524`, chữ và icon `#0B0F14`. OFF nền `#2A3646`, tên `#E6EDF3`, trạng thái
  và icon `#A3AEBB`. Icon theo trường `icon`: `bulb`, `fan`, `plug`, `snow` (hoặc `ac`).
- Chạm: `home_service_set()` gửi trạng thái ngược lại lên `.../set` và đánh dấu pending (ô mờ 40% kèm chữ `...`). **Chỉ đổi màu khi
  nhận `.../state`.** Sau `SWITCH_PENDING_MS` (3 giây) không có phản hồi thì trả về trạng thái cũ.
- Mất MQTT: ô mờ 50%, bỏ qua thao tác chạm, xóa các lệnh đang chờ.
