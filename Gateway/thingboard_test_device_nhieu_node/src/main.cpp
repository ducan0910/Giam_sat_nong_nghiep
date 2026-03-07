#define THINGSBOARD_ENABLE_PROGMEM 0
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <Attribute_Request.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>

#include "LoRaNodeData.h"
#include "ThingsBoardGateway.h"

#include <RadioLib.h>
#include <Ticker.h>

// WiFi credentials
constexpr char WIFI_SSID[] = "bachkhoa";
constexpr char WIFI_PASSWORD[] = "Anhue0631";

// Gateway Token - Use Gateway device token from ThingsBoard
constexpr char TOKEN[] = "SyEiMM672bWt9RQueO1f";

// ThingsBoard server configuration
constexpr char THINGSBOARD_SERVER[] = "103.116.39.179";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

// MQTT and communication settings
constexpr uint32_t MAX_MESSAGE_SIZE = 2048U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;
constexpr size_t MAX_ATTRIBUTES = 4U;
constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

// Shared data between tasks - Array of 100 nodes
LoRaNodeData nodesData[MAX_NODES];
SemaphoreHandle_t dataMutex = NULL;

// New data notification flag (set by LoRa task when new packet received)
volatile bool newDataAvailable = false;
volatile uint8_t updatedNodeId = 0; // 0: không có, 1: Node 1, 2: Node 2

// Gateway shared threshold values (updated from ThingsBoard)
volatile float temperatureThreshold = 30.0;
volatile float humidityThreshold = 80.0;
volatile float soilMoistureThreshold = 50.0;
volatile uint32_t nodeSendInterval = 15;  // Chu kỳ gửi từ node tới gateway (s)

// WiFi and MQTT clients
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);

// Initialize ThingsBoard APIs
Server_Side_RPC<3U, 5U> rpc;
Attribute_Request<2U, MAX_ATTRIBUTES> attr_request;
Shared_Attribute_Update<4U, MAX_ATTRIBUTES> shared_update;

const std::array<IAPI_Implementation*, 3U> apis = {
    &rpc,
    &attr_request,
    &shared_update
};

// Initialize ThingsBoard instance
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE, Default_Max_Stack_Size, Default_Max_Stack_Size, apis);

// Telemetry sending interval
constexpr int16_t telemetrySendInterval = 5000U;

// List of shared attributes for subscribing
constexpr std::array<const char *, 4U> SHARED_ATTRIBUTES_LIST = {
  TEMP_THRESHOLD_ATTR,
  HUM_THRESHOLD_ATTR,
  SOIL_THRESHOLD_ATTR,
  SEND_INTERVAL_ATTR
};

// Attribute callbacks
const Shared_Attribute_Callback<MAX_ATTRIBUTES> attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback<MAX_ATTRIBUTES> attribute_shared_request_callback(&processSharedAttributes, REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut, SHARED_ATTRIBUTES_LIST);

// Task handles
TaskHandle_t thingsBoardTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;

/// @brief Initalizes WiFi connection with timeout
bool InitWiFi() {
  Serial.print("Connecting to WiFi '");
  Serial.print(WIFI_SSID);
  Serial.print("'");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  const int maxAttempts = 20; // 10 seconds timeout
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    return true;
  } else {
    Serial.println(" Failed!");
    return false;
  }
}

/// @brief Reconnects the WiFi when disconnected
bool reconnect() {
  static uint32_t lastCheckTime = 0;
  static bool wasDisconnected = false;
  
  // Check WiFi status every 1 second to avoid excessive checking
  if (millis() - lastCheckTime < 1000) {
    return WiFi.status() == WL_CONNECTED;
  }
  lastCheckTime = millis();
  
  const wl_status_t status = WiFi.status();
  
  if (status == WL_CONNECTED) {
    if (wasDisconnected) {
      Serial.println("✓ WiFi reconnected successfully");
      wasDisconnected = false;
    }
    return true;
  }
  
  // WiFi disconnected
  if (!wasDisconnected) {
    Serial.println("✗ WiFi connection lost! Attempting to reconnect...");
    wasDisconnected = true;
  }
  
  // Try to reconnect
  WiFi.disconnect();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
  return InitWiFi();
}

/// @brief ThingsBoard Task - Runs on Core 0 (WiFi core)
/// Handles all WiFi/MQTT/ThingsBoard communication
void thingsBoardTask(void* parameter) {
  Serial.println("ThingsBoard Task started on Core 0");
  
  // Initialize WiFi
  InitWiFi();
  
  uint32_t previousDataSend = 0;
  bool isConnected = false;
  
  while (true) {
    // Check WiFi connection - reconnect if needed
    if (!reconnect()) {
      Serial.println("Waiting for WiFi reconnection...");
      isConnected = false;
      vTaskDelay(5000 / portTICK_PERIOD_MS);  // Wait 5 seconds before retry
      continue;
    }

    // Connect to ThingsBoard if not connected
    if (!tb.connected()) {
      // WiFi OK nhưng ThingsBoard disconnected
      if (isConnected) {
        Serial.println("✗ ThingsBoard connection lost!");
        isConnected = false;
      }
      
      Serial.print("Connecting to ThingsBoard: ");
      Serial.println(THINGSBOARD_SERVER);
      
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Failed to connect to ThingsBoard");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        continue;
      }
      
      Serial.println("Gateway connected to ThingsBoard!");
      isConnected = true;
      
      // Send Gateway device info
      tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
      tb.sendAttributeData("gatewayType", "ESP32-LoRa");
      
      // Subscribe to Gateway RPC topic
      auto& mqttClient = tb.getClient();
      if (mqttClient.subscribe(GATEWAY_RPC_TOPIC)) {
        Serial.println("Subscribed to Gateway RPC topic");
      }
      
      // Subscribe for shared attributes (thresholds)
      if (!shared_update.Shared_Attributes_Subscribe(attributes_callback)) {
        Serial.println("Failed to subscribe for shared attribute updates");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      }
      
      Serial.println("Subscribed to shared attributes");

      // Request current threshold values from ThingsBoard
      // (Timeout = 5 seconds, nếu ThingsBoard không trả lời sẽ gọi requestTimedOut())
      if (!attr_request.Shared_Attributes_Request(attribute_shared_request_callback)) {
        Serial.println("Failed to request shared attributes");
      } else {
        Serial.println("Requested current threshold values from ThingsBoard");
      }
      
      // NOTE: Không connect nodes ở đây nữa!
      // LoRa task sẽ tự động connect node lên ThingsBoard
      // KHI nhận được packet LoRa thật từ node lần đầu tiên
    }

    // Send telemetry when new data is available from nodes
    if (isConnected && newDataAvailable) {
      newDataAvailable = false;  // Clear flag
      
      // Send node telemetry via Gateway API
      gatewaySendTelemetry(tb);
      
      Serial.println("Telemetry sent to ThingsBoard");
    }
    
    // Forward threshold updates to active nodes (for ThingsBoard Alarm)
    if (isConnected && thresholdUpdatePending) {
      thresholdUpdatePending = false;  // Clear flag
      gatewaySendAttributes(tb);
    }
    
    // Send gateway status periodically (independent from node data)
    if (isConnected && (millis() - previousDataSend > telemetrySendInterval)) {
      previousDataSend = millis();
      tb.sendTelemetryData("rssi", WiFi.RSSI());
      tb.sendTelemetryData("freeHeap", ESP.getFreeHeap());
    }

    // Process ThingsBoard messages
    tb.loop();
    
    // Small delay
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ========== LoRa Task (Core 1) ==========

// LoRa configuration pins (adjust based on your hardware)
#define LORA_NSS  5
#define LORA_DIO0 26
#define LORA_RST  14

// TDMA Constants
#define FRAME_DURATION 42
#define CMD_DATA       0x01
#define CMD_JOIN       0x02
#define CMD_EMERGENCY  0xFF
#define ID_NEWDEVICE  0x00
#define ID_GATEWAY     0xFF

SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST);
Ticker tdmaTimer;

volatile bool receivedFlag = false;          // Cờ ngắt LoRa
volatile uint16_t global_seconds = 0;        // Đồng hồ TDMA
uint8_t next_assigned_id = 0;                // ID tiếp theo sẽ cấp phát cho node mới
uint8_t newID = 0;                           // ID mới được cấp phát

struct __attribute__((packed)) DataPacket {
    uint8_t  ID;
    uint8_t  cmd;
    int16_t  temp;
    uint16_t hum_air;
    uint16_t hum_soil;
    uint16_t battery;
};

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

void onTimerTick() {
  global_seconds++;
  if (global_seconds >= nodeSendInterval) global_seconds = 0;
}

void initLoRa() {
  Serial.begin(115200);
  
  // --- KHỞI TẠO LORA ---
  Serial.print(F("[SX1278] Initializing ... "));
  
  // Khởi tạo: 433.0MHz, BW 125, SF 7, CR 4/5, SyncWord 0x12
  int state = radio.begin(433.0, 125.0, 7, 5, 0x12, 10, 8);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  // Cài đặt ngắt khi nhận gói tin (Sử dụng DIO0)
  radio.setPacketReceivedAction(setFlag);


  // Bắt đầu nghe
  Serial.println(F("GATEWAY SAN SANG!"));
  tdmaTimer.attach(1.0, onTimerTick);
  radio.startReceive();
}

void loraTask(void* parameter) {
  Serial.println("LoRa Task started on Core 1");
  
  // Initialize LoRa radio
  initLoRa();
  
  // Array to track last received time for each node (for timeout detection)
  uint32_t lastNodeReceived[MAX_NODES];
  for(int i=0; i<MAX_NODES; i++) lastNodeReceived[i] = millis();
  
  // ===== LOGIC KẾT NỐI NODE VÀO MẠNG =====
  // 1. Node khởi động lần đầu → gửi JOIN_REQUEST (ID=0x00, CMD=0xJJ) đến Gateway
  // 2. Gateway nhận JOIN_REQUEST → cấp phát ID cho node (first come first served)
  //    - Node đầu tiên → ID = 1
  //    - Node thứ hai → ID = 2
  //    - ... Node thứ 100 → ID = 100
  // 3. Gateway gửi ACK cho phép gia nhập kèm ID đã cấp:
  //    Packet format: [0xFF (Gateway)][Node ID][0xAC (ACK)][Thresholds...]
  // 4. Node nhận ACK → lưu ID của mình → bắt đầu gửi data định kỳ
  // 5. Các lần gửi sau, node luôn dùng ID đã được cấp
  //    - Node ID từ 1-100 tương ứng với index 0-99 trong mảng nodesData[]
  
  while (true) {
    if(receivedFlag) {
      receivedFlag = false; // Xóa cờ

      DataPacket rxData;
      int state = radio.readData((uint8_t*)&rxData, sizeof(DataPacket));
      int size_data = radio.getPacketLength();
      if (state == RADIOLIB_ERR_NONE && rxData.ID != ID_GATEWAY && size_data >= 3 && size_data <= sizeof(DataPacket)) {
        

        // Cập nhật thời gian nhận dữ liệu cuối cho node tương ứng          
        uint8_t timeH = (uint8_t)(global_seconds >> 8);
        uint8_t timeL = (uint8_t)(global_seconds & 0xFF);
        uint8_t tempVal = (uint8_t)temperatureThreshold;
        uint8_t humAVal = (uint8_t)humidityThreshold;
        uint8_t humSVal = (uint8_t)soilMoistureThreshold;
        uint8_t timeIntervalH = (uint8_t)(nodeSendInterval >> 8);
        uint8_t timeIntervalL = (uint8_t)(nodeSendInterval & 0xFF);

        if(rxData.cmd == CMD_JOIN) {
          Serial.printf("[JOIN] Yeu cau tu ID: 0x%02X vao giay thu %d\n", rxData.ID, global_seconds);
        
          // Delay để STM32 kịp chuyển sang nhận
          vTaskDelay(20 / portTICK_PERIOD_MS);
          newID++;
          nodesData[newID - 1].nodeId = newID; // Lưu ID mới vào mảng node data
          nodesData[newID - 1].connected = true;
          nodesData[newID - 1].registered = true; 
          nodesData[newID - 1].lastUpdate = millis();
          lastNodeReceived[newID - 1] = millis();

          // Gửi ACK kèm ID đã cấp và thresholds
          uint8_t ack[] = {ID_GATEWAY, rxData.ID, newID, timeH, timeL, 
                             tempVal, 0, humAVal, 10, humSVal, 20, timeIntervalH, timeIntervalL};
          radio.transmit(ack, sizeof(ack));
          Serial.printf("[JOIN]Da gui ACK va cap ID: 0x%02X cho Node\n", newID);
          char nodeName[16];
          getNodeName(newID, nodeName, sizeof(nodeName));
          gatewayConnectNode(tb, nodeName);

        }
        
        else if(rxData.cmd == CMD_DATA || rxData.cmd == CMD_EMERGENCY) {
          if(rxData.cmd == CMD_EMERGENCY) Serial.println(F("!!! TIN HIEU VUOT NGUONG !!!"));
          else if(rxData.cmd == CMD_DATA) Serial.println(F("[DATA] Nhan du lieu binh thuong"));

          Serial.println(F("--- DATA SENSOR ---"));
          Serial.printf("Giay thu: %d\n", global_seconds);
          Serial.printf("ID Node:   0x%02X\n", rxData.ID);
          Serial.printf("Nhiet do:  %.2f C\n", rxData.temp / 10.0);
          Serial.printf("Do am KK:  %.2f %%\n", rxData.hum_air / 10.0);
          Serial.printf("Do am Dat: %.2f %%\n", rxData.hum_soil / 10.0);
          Serial.printf("Pin:       %u mV\n", rxData.battery);
          Serial.printf("RSSI:      %.2f dBm\n", radio.getRSSI());
          Serial.printf("SNR:       %.2f dB\n", radio.getSNR());
          Serial.println(F("-------------------"));

          uint8_t idx = rxData.ID - 1; // Chuyển ID thành index mảng (0-99)
          // Cập nhật dữ liệu vào mảng nodesData
          if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            nodesData[idx].temperature = rxData.temp / 10.0;
            nodesData[idx].humidity = rxData.hum_air / 10.0;
            nodesData[idx].soilMoisture = rxData.hum_soil / 10.0;
            nodesData[idx].batteryVoltage = rxData.battery / 1000.0; // mV to V
            nodesData[idx].rssi = radio.getRSSI();
            nodesData[idx].snr = radio.getSNR();
            nodesData[idx].lastUpdate = millis();
            nodesData[idx].connected = true;
            nodesData[idx].hasNewData = true;  // Đánh dấu node có data mới
            lastNodeReceived[idx] = millis();
            xSemaphoreGive(dataMutex);
          }
          updatedNodeId = rxData.ID; // Đánh dấu node có data mới
          newDataAvailable = true;   // Đánh dấu có data mới từ node

          // Gửi ACK kèm thresholds
          uint8_t ack[] = {ID_GATEWAY, rxData.ID, timeH, timeL, 
                          tempVal, 0, humAVal, 10, humSVal, 20, timeIntervalH, timeIntervalL};
          radio.transmit(ack, 12);
        }
      }
      else if(state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F(">> CRC MISMATCH!"));
      }

      radio.startReceive();
    }

    // ========== CHECK NODE TIMEOUTS ==========
    // Node disconnects when: (current time - last received) > (nodeSendInterval + 20s)
    // nodeSendInterval from shared attribute (unit: seconds)
    uint32_t dynamicTimeout = (nodeSendInterval + 20) * 1000; // Convert to milliseconds

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      for (uint8_t i = 0; i < MAX_NODES; i++) {
        if (nodesData[i].connected && lastNodeReceived[i] > 0) {
          uint32_t timeSinceLastReceived = millis() - lastNodeReceived[i];
          if (timeSinceLastReceived > dynamicTimeout) {
            uint8_t nodeId = i + 1;
            nodesData[i].connected = false;
            
            Serial.println("\n🔴🔴🔴 TIMEOUT DETECTED! 🔴🔴🔴");
            Serial.print("Node");
            Serial.print(nodeId);
            Serial.print(" disconnected (timeout: ");
            Serial.print(timeSinceLastReceived / 1000);
            Serial.print("s > ");
            Serial.print(dynamicTimeout / 1000);
            Serial.println("s)");
            
            // Gửi disconnect message lên ThingsBoard
            if (tb.connected() && nodesData[i].registered) {
              xSemaphoreGive(dataMutex);  // Release mutex before TB call
              
              char nodeName[16];
              getNodeName(nodeId, nodeName, sizeof(nodeName));
              gatewayDisconnectNode(tb, nodeName);
              
              xSemaphoreTake(dataMutex, portMAX_DELAY);  // Take again
            }
          }
        }
      }
      xSemaphoreGive(dataMutex);
    }
    
    // Small delay to prevent busy waiting
    vTaskDelay(1);
  }
}

// ========== Setup & Loop ==========

void setup() {
  // Initialize serial for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 LoRa Gateway - Dual Core");
  Serial.println("=================================");
  
  // Create mutex for shared data protection
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == NULL) {
    Serial.println("Failed to create mutex!");
    while (1);
  }
  
  // Initialize all nodes data to zero
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    nodesData[i] = {0, 0, 0, 0, 0, 0, 0, false, false, false, 0};
  }
  
  // Create ThingsBoard Task on Core 0 (WiFi core)
  xTaskCreatePinnedToCore(
    thingsBoardTask,           // Task function
    "ThingsBoard",             // Task name
    8192,                      // Stack size (bytes)
    NULL,                      // Parameters
    1,                         // Priority
    &thingsBoardTaskHandle,    // Task handle
    0                          // Core 0 (WiFi core)
  );
  
  // Create LoRa Task on Core 1 (App core)
  xTaskCreatePinnedToCore(
    loraTask,                  // Task function
    "LoRa",                    // Task name
    4096,                      // Stack size (bytes)
    NULL,                      // Parameters
    1,                         // Priority
    &loraTaskHandle,           // Task handle
    1                          // Core 1 (App core)
  );
  
  Serial.println("Tasks created successfully");
  Serial.println("Core 0: ThingsBoard (WiFi/MQTT)");
  Serial.println("Core 1: LoRa Communication");
}

void loop() {
  // Empty - all work done in FreeRTOS tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
