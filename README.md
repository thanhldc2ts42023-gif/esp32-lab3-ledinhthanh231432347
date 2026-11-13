# Hệ thống Giám sát và Cảnh báo Té ngã ESP32

Hệ thống phát hiện té ngã và giám sát thông số gia tốc, góc nghiêng theo thời gian thực sử dụng vi điều khiển ESP32, cảm biến MPU6050, giao thức MQTT và giao diện điều khiển Web Dashboard.

## Phần cứng và Môi trường
* **Vi điều khiển:** ESP32 DevKit v1 (lập trình đa nhiệm FreeRTOS trên PlatformIO)
* **Cảm biến:** MPU6050 (đọc giá trị gia tốc 3 trục và góc nghiêng qua giao tiếp I2C)
* **Thiết bị cảnh báo:** Đèn LED và Còi Buzzer
* **Mô phỏng:** Wokwi Simulator

## Quy trình hoạt động (FreeRTOS Dual-Core)

1. **Thu thập dữ liệu (`Task đọc data` - Core 1)**
   * Đọc liên tục thông số gia tốc (Ax, Ay, Az) và góc nghiêng (Roll, Pitch) từ MPU6050.
   * Phân tích ngưỡng té ngã và đẩy dữ liệu vào hai hàng đợi: `dataQueue` và `dataAlert`.

2. **Xử lý cảnh báo (`Task cảnh báo` - Core 1)**
   * Nhận tín hiệu từ `dataAlert` để tự động bật còi và đèn LED khi phát hiện té ngã.
   * Hỗ trợ bật/tắt còi và đèn thủ công (Manual) từ giao diện Web.

3. **Truyền nhận dữ liệu (`Task truyền data` - Core 0)**
   * Kết nối WiFi và gửi dữ liệu từ `dataQueue` lên MQTT Broker (`broker.hivemq.com`).
   * Nhận lệnh điều khiển từ xa gửi về từ Web Dashboard.

4. **Giao diện giám sát (`index.html`)**
   * Kết nối trực tiếp tới MQTT Broker qua giao thức WebSockets.
   * Hiển thị trạng thái thiết bị (Online/Offline), biểu đồ thông số (Chart.js), cảnh báo té ngã tức thì và nút điều khiển thiết bị.

## Cấu trúc thư mục
* `main.cpp` - Mã nguồn firmware điều khiển ESP32 (FreeRTOS + MQTT)
* `index.html` - Giao diện Web Dashboard giám sát thời gian thực
* `diagram.json` - Sơ đồ kết nối phần cứng trên Wokwi
* `platformio.ini` - Cấu hình thư viện PlatformIO
* `wokwi.toml` - Cấu hình môi trường mô phỏng Wokwi
