#ifndef LORA_NODE_DATA_H
#define LORA_NODE_DATA_H

#include <Arduino.h>

// Maximum number of LoRa nodes supported
#define MAX_NODES 100

// LoRa Node data structure
struct LoRaNodeData {
  float temperature;      // Nhiệt độ không khí (°C)
  float humidity;         // Độ ẩm không khí (%)
  float soilMoisture;     // Độ ẩm đất (%)
  float batteryVoltage;   // Điện áp pin (V)
  int16_t rssi;           // Chỉ số cường độ tín hiệu (dBm)
  float snr;              // Tỉ lệ tín hiệu/nhiễu (dB)
  uint32_t lastUpdate;
  bool connected;
  bool registered;        // Node đã được đăng ký trên ThingsBoard
  bool hasNewData;        // Node có data mới chưa gửi lên ThingsBoard
  uint8_t nodeId;         // ID của node (1-100)
};

// Shared data between tasks (protected by mutex)
// Array of 100 nodes (index 0 = Node ID 1, index 99 = Node ID 100)
extern LoRaNodeData nodesData[MAX_NODES];
extern SemaphoreHandle_t dataMutex;

// Shared threshold values (updated from ThingsBoard)
extern volatile float temperatureThreshold;
extern volatile float humidityThreshold;
extern volatile float soilMoistureThreshold;
extern volatile uint32_t nodeSendInterval;

#endif // LORA_NODE_DATA_H
