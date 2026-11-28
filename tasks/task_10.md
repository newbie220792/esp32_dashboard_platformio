# task_10.md — Tab Photos: chiếu ảnh slide từ Pi 5

Board: Guition JC4827W543C (480x272, LVGL 9.x, PlatformIO + Arduino). Làm tiếp sau `task_9.md`, giữ kiến trúc phân tầng ở `task_7.md`. Thiết kế nằm trên canvas, màn **10 · Photos slideshow** và **11 · Photos: controls**.

## 1. Mục tiêu và phạm vi
- Thêm **tab thứ 5 "Photos"**: chiếu ảnh từ Pi 5 dạng slide, mỗi ảnh hiển thị **5 hoặc 10 giây** (chọn được).
- **Pi 5 lo toàn bộ việc xử lý ảnh** (cắt, co về 480x272, nén JPEG). ESP32 chỉ tải, giải mã và hiển thị.
- **Ngoài phạm vi:** video, GIF, âm thanh, lưu ảnh trong flash. Không thay đổi các tab và màn phủ hiện có, trừ việc thêm chấm trang thứ 5.
- **Đã chốt bỏ OTA.** Task này đổi bảng phân vùng sang `huge_app.csv` (mục 5.5) và gỡ mã OTA. Từ giờ chỉ nạp firmware qua cáp USB.
- **Pi 5 quyết định hoàn toàn ảnh nào được chiếu, theo thứ tự nào.** ESP32 không lọc, không xáo, không chọn thư mục.

## 2. Giao diện

### 2.1 Tab Photos (màn 10)
- Ảnh **phủ kín 480x272**, không có thanh trạng thái. Đây là màn duy nhất trong 5 tab không có thanh trạng thái, nên Wi-Fi và giờ chỉ thấy khi hiện thanh điều khiển.
- Góc dưới phải có nhãn nhỏ `3 / 24` (cao 22px, bo 11, nền `rgba(11,15,20,.55)`, chữ 12 đậm). Số thứ tự trong danh sách hiện tại.
- Vuốt ngang để đổi tab như các tab khác. **Chỉ đổi ảnh bằng nút Prev/Next** ở màn 11 (hoặc tự động theo giờ). Không đổi ảnh bằng vuốt hay chạm vào mép màn hình.
- Chấm trang: cả 5 tab dùng 5 chấm (tab đang chọn dài 20px, các chấm còn lại rộng 8px, cao 4px, cách nhau 6px). **Cập nhật chấm trang ở 4 tab cũ** từ số chấm hiện tại lên 5 chấm.

### 2.2 Thanh điều khiển (màn 11)
- **Chạm vào ảnh** để hiện, **tự ẩn sau 3 giây** không chạm (đặt lại bộ đếm mỗi lần chạm nút).
- Thanh trên cao 56px, nền chuyển màu từ `rgba(11,15,20,.8)` về trong suốt: nhãn `Photos` bên trái, 5 chấm trang ở giữa, biểu tượng Wi-Fi và giờ bên phải.
- Thanh dưới cao 84px, nền chuyển màu ngược lại, padding ngang 12px:
  - Trái: nút `10s` (cao 32px, bo 16px). **Chạm để đổi 5s ↔ 10s**, lưu NVS khóa `photo_s`.
  - Giữa: nút `Prev` 44px, nút `Pause/Play` 52px (nền `#3DD6C6`, biểu tượng tối `#0B0F14`), nút `Next` 44px. Khoảng cách 14px.
  - Phải: bộ đếm `3 / 24`.
- Vùng chạm tối thiểu 44px.
- Khi chạm vào ảnh lần nữa (ngoài các nút), thanh ẩn ngay.
- Trong lúc thanh điều khiển đang hiện, đồng hồ đếm giây của slide **tạm dừng** để không đổi ảnh ngay khi người dùng đang bấm.

### 2.3 Trạng thái đặc biệt (cần dựng thêm, chưa có trên canvas)
- **Chưa có ảnh** (`count = 0`): nền `#0B0F14`, biểu tượng ảnh xám `#6B7788` và dòng `No photos` ở giữa.
- **Mất kết nối Pi 5 hoặc Wi-Fi**: **giữ nguyên ảnh đang hiển thị**, thêm chip nhỏ ở góc trên phải `Offline` (chấm đỏ). Slide dừng đổi ảnh cho đến khi có mạng lại.
- **Đang tải ảnh đầu tiên**: nền tối với vòng xoay nhỏ, tối đa vài giây.
- Chữ trên màn chỉ dùng **ASCII**, không thêm font (đã chốt ở `task_8.md`).

## 3. API của Pi 5 (REST, cùng Bearer token như `task_8.md`)

| Method | Đường dẫn | Mục đích |
|---|---|---|
| GET | `/api/photos` | Danh sách ảnh sẽ chiếu. |
| GET | `/api/photos/{id}.jpg` | Nội dung một ảnh đã xử lý sẵn. |

`GET /api/photos` trả:
```json
{"rev":"a41f9c","count":24,"items":[{"id":"p001"},{"id":"p002"}]}
```
- `rev`: mã đổi khi tập ảnh hoặc thứ tự đổi. ESP32 so sánh với lần trước để biết có phải tải lại danh sách không.
- `items`: **tối đa 100 ảnh**. Thứ tự là thứ tự sẽ chiếu. **Pi 5 quyết định toàn bộ**: ảnh nào, theo thứ tự nào (tuần tự hay xáo), thư mục nào. ESP32 chiếu đúng theo danh sách nhận được.
- `id`: chuỗi ngắn (tối đa 12 ký tự ASCII), không chứa đường dẫn hay tên file gốc.

`GET /api/photos/{id}.jpg`:
- Phải có header **`Content-Length`** (không dùng chunked), để ESP32 biết kích thước và cấp đúng bộ đệm.
- Trả `404` nếu `id` không còn. ESP32 bỏ qua ảnh đó và sang ảnh kế tiếp.
- Nên có `ETag` hoặc `Cache-Control` cho trình duyệt thử, nhưng ESP32 không cần dùng.

## 4. Xử lý ảnh ở Pi 5 (yêu cầu bắt buộc)
ESP32 **không** co hay xoay ảnh, nên ảnh trả về phải đúng định dạng sau:
- Kích thước **480x272** đúng từng pixel. Cắt kiểu **cover** (phủ kín khung, cắt phần thừa ở giữa); tỷ lệ 480:272 khoảng 1.76. Có thể tùy chọn tâm cắt lệch lên (cắt theo vùng khuôn mặt) nếu muốn.
- Áp dụng hướng xoay theo EXIF rồi **xóa EXIF** khỏi bản trả về.
- Chuyển về hệ màu **sRGB**.
- JPEG **baseline** (không progressive), lấy mẫu màu 4:2:0, chất lượng khoảng 80. **Không dùng progressive** vì thư viện giải mã trên ESP32 không hỗ trợ.
- Dung lượng mục tiêu **dưới 60 KB**, **tối đa 90 KB**. Nếu vượt thì Pi 5 giảm chất lượng dần cho đến khi đạt.
- Pi 5 xử lý **một lần** và lưu bản đã xử lý (thư mục bộ nhớ đệm), chỉ xử lý lại khi ảnh gốc đổi (so sánh thời gian sửa hoặc hàm băm). **Không xử lý theo từng yêu cầu** để không làm chậm mỗi lần ESP32 tải.
- Công cụ gợi ý: Pillow (Python) hoặc ImageMagick. Quét thư mục ảnh gốc định kỳ hoặc dùng `inotify`.
- Thư mục ảnh gốc, kiểu thứ tự (tuần tự hay xáo), lọc ảnh và giới hạn 100 ảnh là **cấu hình ở Pi 5**, không hard-code ở ESP32. Đổi cấu hình thì `rev` đổi và ESP32 tự cập nhật ở lần tải danh sách kế tiếp.

## 5. Phía ESP32

### 5.1 Luồng hoạt động
1. Khi tab Photos được chọn lần đầu (hoặc khi mở lại sau khi rời), gọi `GET /api/photos`. Nếu `rev` không đổi so với lần trước thì dùng lại danh sách cũ.
2. Tải ảnh số 1 → giải mã → hiển thị. Ngay sau đó tải trước ảnh kế tiếp trong lúc ảnh hiện tại đang hiển thị.
3. Mỗi 5 hoặc 10 giây (theo `photo_s`), chuyển sang ảnh đã tải sẵn bằng hiệu ứng mờ dần khoảng 300 ms, rồi tải ảnh kế tiếp.
4. Hết danh sách thì quay lại đầu danh sách. Khi quay lại đầu, gọi lại `GET /api/photos` để cập nhật `rev`.
5. **Chỉ tải khi tab Photos đang hiển thị.** Rời tab thì dừng tải và dừng đếm giây, để nhường Wi-Fi và CPU cho MQTT và REST ở các tab khác. Quay lại thì tiếp tục từ ảnh đang xem.
6. Nút Prev quay lại ảnh trước trong danh sách, tải lại ảnh đó nếu cần (không giữ lịch sử ảnh đã giải mã). Nút Next đổi ngay sang ảnh kế tiếp. Chỉ số ảnh quay vòng.
7. Ảnh lỗi (HTTP khác 200, quá kích thước, giải mã thất bại): bỏ qua, sang ảnh kế tiếp ngay, đếm số lần lỗi liên tiếp. Quá 3 lần liên tiếp thì chuyển sang trạng thái `Offline` và thử lại sau 15 giây.

### 5.2 Giải mã và bộ nhớ
- Thư viện giải mã: **JPEGDEC** (bitbank2) hoặc TJpg_Decoder. Chọn một và ghi vào `platformio.ini`. Bắt buộc hỗ trợ baseline JPEG.
- Bộ nhớ trong **PSRAM** (không dùng RAM trong):
  - 1 bộ đệm tải JPEG: **96 KB** (tối đa kích thước Pi 5 cho phép).
  - 2 bộ đệm khung RGB565 480x272: mỗi bộ **261 120 byte** (khoảng 255 KB), tổng khoảng 510 KB. Một bộ đang hiển thị, một bộ để giải mã ảnh kế tiếp.
  - Cấp phát một lần khi vào tab lần đầu bằng `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` và giữ lại. Nếu cấp phát thất bại thì hiện `No memory` và không tải ảnh.
- Hiển thị bằng `lv_image` trỏ thẳng vào bộ đệm RGB565 (qua `lv_image_dsc_t`), **không sao chép thêm**. Hiệu ứng mờ dùng hai `lv_image` chồng nhau và hoạt ảnh độ trong suốt.
- **Thứ tự byte và màu:** board dùng NV3041A cần xử lý đảo byte và đảo màu (đã ghi ở các task trước). Đảm bảo giải mã ra đúng thứ tự byte mà đường vẽ LVGL của bạn đang dùng, và kiểm tra bằng một ảnh có màu đỏ, xanh lá, xanh dương rõ ràng.
- Giải mã ảnh 480x272 mất khoảng 30–50 ms, nên diễn ra trong task riêng, không trong task LVGL. Kết quả giao cho UI qua cờ "khung mới đã sẵn sàng" và `lv_timer` đổi con trỏ.

### 5.3 Mạng
- Dùng `HTTPClient` với `WiFiClient` (HTTP trong LAN, như `task_8.md`). Timeout 5 giây cho `GET /api/photos`, 8 giây cho mỗi ảnh.
- Kiểm tra `Content-Length` hợp lệ và không vượt 96 KB trước khi đọc. Đọc đúng số byte đó vào bộ đệm, không đọc vô hạn.
- Chỉ gọi khi Wi-Fi đã có IP. Mất Wi-Fi thì dừng và giữ ảnh hiện tại.
- Task HTTP có stack khoảng 8 KB, ưu tiên thấp hơn task LVGL.
- Không chạy song song với yêu cầu của Alerts hay System info nếu gây nghẽn. Có thể dùng chung một hàng đợi yêu cầu HTTP.

### 5.4 Màn hình luôn sáng khi chạy slide
- **Khi tab Photos đang hiển thị, đèn nền không bao giờ tắt** và không tự giảm sáng, bất kể cài đặt "Screen off" ở Settings. Độ sáng vẫn theo thanh Brightness.
- Cách làm gợi ý: khi tab Photos được chọn, tạm vô hiệu bộ đếm không hoạt động (hoặc gọi `lv_display_trigger_activity` định kỳ, ví dụ mỗi 5 giây); khi rời tab, **khôi phục đúng hành vi cũ** theo giá trị `sleep_s` đã lưu và đặt lại bộ đếm từ lúc rời tab. Không ghi đè giá trị `sleep_s` trong NVS.
- Giữ sáng cả khi đang `Offline`, vì màn vẫn hiển thị ảnh cuối cùng.
- Nếu đèn nền đang tắt (do "Screen off" trước đó) lúc người dùng vuốt sang tab Photos: chạm đầu tiên chỉ đánh thức màn như thường lệ; từ lúc đó tab Photos giữ sáng.

### 5.5 Bỏ OTA và đổi bảng phân vùng
- Trong `platformio.ini` đặt `board_build.partitions = huge_app.csv` (bảng phân vùng có sẵn của Arduino-ESP32, một vùng app khoảng 3 MB, không có hai vùng OTA). Xóa tham chiếu tới `partitions_ota.csv` và file đó nếu còn trong dự án.
- Gỡ mã OTA còn lại: `ArduinoOTA`, `HTTPUpdate`, kiểm tra rollback và các dòng cấu hình liên quan (`task_5.md`). Không để lại hàm chết.
- Đổi bảng phân vùng làm thay đổi bố cục flash, nên **nạp bằng cáp USB và xóa flash một lần** (`pio run -t erase`) trước khi nạp lại cho sạch. Việc này **xóa NVS**: Wi-Fi đã lưu, độ sáng, `sleep_s`, `photo_s` quay về mặc định, cần cấu hình lại Wi-Fi (qua `secrets.h` hoặc trang cấu hình).
- Dung lượng firmware không còn bị giới hạn 1.94 MB. Ở màn System info (`task_9.md`), thanh **Firmware size** hiển thị tỷ lệ so với kích thước vùng app mới (khoảng 3 MB). Nếu `ESP.getFreeSketchSpace()` không còn trả về giá trị phù hợp, dùng kích thước phân vùng app đọc từ bảng phân vùng.

## 6. Phân tầng (theo `task_7.md`)
- `services/photo_service.*`: quản lý danh sách, tải ảnh, giải mã, hai bộ đệm, đếm giây, trạng thái (đang phát, tạm dừng, offline, rỗng). **Không gọi LVGL.**
- `ui/screen_photos.*`: dựng tab, hai `lv_image`, thanh điều khiển, nhãn đếm, `lv_timer` kiểm tra "khung mới sẵn sàng". Chỉ tầng `ui/` gọi LVGL.
- `ui/tabview` (hoặc tương đương): đăng ký tab thứ 5 và cập nhật chấm trang.
- `hal/`: nếu có hàm cấp phát PSRAM, giữ ở đây.
- Giao tiếp giữa task photo và UI qua state có khóa mutex và sự kiện, như các dịch vụ khác.
- Khóa NVS mới: `photo_s` (5 hoặc 10, mặc định 10).

## 7. Các bước thực hiện
1. Pi 5: viết bộ xử lý ảnh (cắt, co, nén, bộ nhớ đệm) và hai endpoint ở mục 3. Kiểm thử bằng `curl` trước khi làm phía ESP32.
2. ESP32: đổi sang `huge_app.csv` và gỡ mã OTA (mục 5.5), xóa flash và nạp lại bằng USB; thêm thư viện giải mã JPEG vào `platformio.ini`.
3. Viết `photo_service`: tải danh sách, tải một ảnh, giải mã vào PSRAM.
4. Dựng `screen_photos`: hiển thị một ảnh tĩnh từ bộ đệm, kiểm tra màu và thứ tự byte.
5. Thêm vòng slide: đếm giây, tải trước ảnh kế tiếp, hiệu ứng mờ.
6. Thêm thanh điều khiển: chạm hiện/ẩn, Prev, Pause/Play, Next, nút `5s/10s`, lưu NVS. Thêm cơ chế giữ sáng khi ở tab Photos (mục 5.4).
7. Thêm trạng thái rỗng, đang tải, Offline.
8. Thêm tab vào dải vuốt và cập nhật chấm trang ở cả 5 tab.
9. Thử rời tab và quay lại, mất Wi-Fi giữa chừng, Pi 5 tắt, ảnh hỏng, danh sách rỗng, danh sách đổi `rev` khi đang chạy.
10. Đo bộ nhớ: heap trong, PSRAM và bộ nhớ LVGL trước và sau khi chiếu nhiều chu kỳ.
11. Cập nhật `CLAUDE.md` và `docs/` (API ảnh, yêu cầu định dạng ảnh, khóa NVS).

## 8. Tiêu chí hoàn thành
- Tab Photos chiếu liên tục các ảnh từ Pi 5, mỗi ảnh 5 hoặc 10 giây, ảnh đúng màu, không vỡ hình, không sọc.
- Chuyển ảnh mượt (mờ dần), không giật màn hình và không khựng đồng hồ ở các tab khác.
- Prev, Next, Pause, Play và nút 5s/10s hoạt động; lựa chọn 5s/10s được giữ sau khi khởi động lại.
- Thanh điều khiển hiện khi chạm và tự ẩn sau 3 giây.
- Rời tab thì ngừng tải; quay lại thì tiếp tục.
- Ở tab Photos đèn nền không tắt dù "Screen off" là 15s; rời tab thì "Screen off" hoạt động lại bình thường và không đổi giá trị đã lưu.
- Đổi ảnh chỉ bằng nút Prev/Next (và tự động theo giờ); vuốt ngang chỉ đổi tab.
- Không còn mã OTA và `partitions_ota.csv`; build dùng `huge_app.csv`.
- Mất mạng hoặc Pi 5 tắt: màn không treo, giữ ảnh hiện tại, hiện `Offline`; có mạng lại thì tự chạy tiếp. Ảnh hỏng không làm treo vòng slide.
- Chiếu liên tục vài giờ không làm tụt heap trong, PSRAM hay bộ nhớ LVGL (không rò rỉ).
- `pio run` build thành công, không warning mới.

## 9. Đã chốt (không còn câu hỏi mở)
- Màn hình **luôn sáng** khi đang ở tab Photos.
- **Pi 5 quyết định** ảnh và thứ tự chiếu.
- **Bỏ OTA**, dùng `huge_app.csv`, nạp bằng USB.
- Đổi ảnh **chỉ qua nút Prev/Next**; vuốt ngang chỉ đổi tab.