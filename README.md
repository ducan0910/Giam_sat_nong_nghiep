**Agricultural IoT Monitoring System:** 
Hệ thống giám sát nông nghiệp thông minh dựa trên mạng cảm biến không dây (Wireless Sensor Network - WSN), sử dụng công nghệ truyền thông LoRa để tối ưu hóa khoảng cách và tiết kiệm năng lượng.
Dự án này xây dựng một hệ thống thu thập dữ liệu môi trường từ các Node cảm biến và gửi về Gateway trung tâm để quản lý và hiển thị trực quan trên Dashboard.
- Node cảm biến: Sử dụng STM32 để đọc dữ liệu từ các cảm biến và truyền qua module LoRa.
- Gateway: Sử dụng ESP32 để nhận dữ liệu LoRa và đẩy lên Cloud qua Wi-Fi/MQTT.
- Dashboard: Hiển thị dữ liệu thực tế thông qua ThingsBoard.
**Tính năng chính**
- Thu thập dữ liệu môi trường: Nhiệt độ, độ ẩm không khí, độ ẩm đất.
- Truyền tin không dây khoảng cách xa với công nghệ LoRa.
