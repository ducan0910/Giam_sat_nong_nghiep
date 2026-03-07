#include "ThingsBoardGateway.h"
#include <ArduinoJson.h>

// Gateway API Topics
const char GATEWAY_TELEMETRY_TOPIC[] = "v1/gateway/telemetry";
const char GATEWAY_ATTRIBUTES_TOPIC[] = "v1/gateway/attributes";
const char GATEWAY_CONNECT_TOPIC[] = "v1/gateway/connect";
const char GATEWAY_DISCONNECT_TOPIC[] = "v1/gateway/disconnect";
const char GATEWAY_RPC_TOPIC[] = "v1/gateway/rpc";

// LoRa Node configuration
const char DEVICE_TYPE[] = "LoRaSensor";

// Helper function to generate node name dynamically
void getNodeName(uint8_t nodeId, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "LoRaNode%d", nodeId);
}

// Attribute names
const char TEMP_THRESHOLD_ATTR[] = "temperatureThreshold";
const char HUM_THRESHOLD_ATTR[] = "humidityThreshold";
const char SOIL_THRESHOLD_ATTR[] = "soilMoistureThreshold";
const char SEND_INTERVAL_ATTR[] = "sendInterval";

// Flag for threshold update forwarding
volatile bool thresholdUpdatePending = false;

void gatewayConnectNode(ThingsBoard& tb, const char* nodeName) {
  // Step 1: Send connect message
  StaticJsonDocument<128> doc;
  doc["device"] = nodeName;
  doc["type"] = DEVICE_TYPE;
  
  char buffer[128];
  size_t len = serializeJson(doc, buffer);
  
  auto& client = tb.getClient();
  if (client.publish(GATEWAY_CONNECT_TOPIC, (uint8_t*)buffer, len)) {
    Serial.print("Gateway: Connected node ");
    Serial.println(nodeName);
    
    // Step 2: Send active=true attribute để mark device as active
    StaticJsonDocument<256> attrDoc;
    JsonObject nodeAttr = attrDoc.createNestedObject(nodeName);
    nodeAttr["active"] = true;
    
    char attrBuffer[256];
    size_t attrLen = serializeJson(attrDoc, attrBuffer);
    
    if (client.publish(GATEWAY_ATTRIBUTES_TOPIC, (uint8_t*)attrBuffer, attrLen)) {
      Serial.print("📊 Sent active=true attribute for ");
      Serial.println(nodeName);
    }
  }
}

void gatewayDisconnectNode(ThingsBoard& tb, const char* nodeName) {
  // Step 1: Send disconnect message
  StaticJsonDocument<128> doc;
  doc["device"] = nodeName;
  
  char buffer[128];
  size_t len = serializeJson(doc, buffer);
  
  Serial.print("📤 Sending DISCONNECT: ");
  Serial.print(buffer);
  Serial.print(" to topic: ");
  Serial.println(GATEWAY_DISCONNECT_TOPIC);
  
  auto& client = tb.getClient();
  if (client.publish(GATEWAY_DISCONNECT_TOPIC, (uint8_t*)buffer, len)) {
    Serial.print("✅ Gateway: Disconnected node ");
    Serial.println(nodeName);
    
    // Step 2: Send active=false attribute để force update status
    StaticJsonDocument<256> attrDoc;
    JsonObject nodeAttr = attrDoc.createNestedObject(nodeName);
    nodeAttr["active"] = false;
    
    char attrBuffer[256];
    size_t attrLen = serializeJson(attrDoc, attrBuffer);
    
    if (client.publish(GATEWAY_ATTRIBUTES_TOPIC, (uint8_t*)attrBuffer, attrLen)) {
      Serial.print("📊 Sent active=false attribute for ");
      Serial.println(nodeName);
    }
  } else {
    Serial.print("❌ Failed to disconnect node ");
    Serial.println(nodeName);
  }
}

void gatewaySendTelemetry(ThingsBoard& tb) {
  // Send telemetry in batches to avoid exceeding MQTT packet size limit
  // Maximum ~10 nodes per batch (safe limit for 2KB message size)
  const uint8_t BATCH_SIZE = 10;
  
  auto& client = tb.getClient();
  
  // Lock mutex to safely read node data
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    uint8_t batchCount = 0;
    StaticJsonDocument<2048> doc;
    char nodeName[16];
    
    for (uint8_t i = 0; i < MAX_NODES; i++) {
      // CHỈ GỬI TELEMETRY CHO NODE CÓ DATA MỚI
      if (nodesData[i].connected && nodesData[i].registered && nodesData[i].hasNewData) {
        uint8_t nodeId = i + 1; // Node ID = index + 1
        getNodeName(nodeId, nodeName, sizeof(nodeName));
        
        // Create telemetry array for this node
        JsonArray nodeArray = doc.createNestedArray(nodeName);
        JsonObject nodeObj = nodeArray.createNestedObject();
        nodeObj["id"] = nodeId;
        nodeObj["temperature"] = nodesData[i].temperature;
        nodeObj["humidity"] = nodesData[i].humidity;
        nodeObj["soilMoisture"] = nodesData[i].soilMoisture;
        nodeObj["batteryVoltage"] = nodesData[i].batteryVoltage;
        nodeObj["rssi"] = nodesData[i].rssi;
        nodeObj["snr"] = nodesData[i].snr;
        
        // Clear flag sau khi đã gửi
        nodesData[i].hasNewData = false;
        
        batchCount++;
        
        // Send batch when limit reached
        if (batchCount >= BATCH_SIZE) {
          char buffer[2048];
          size_t len = serializeJson(doc, buffer);
          client.publish(GATEWAY_TELEMETRY_TOPIC, (uint8_t*)buffer, len);
          
          // Reset for next batch
          doc.clear();
          batchCount = 0;
        }
      }
    }
    
    // Send remaining nodes in last batch
    if (batchCount > 0) {
      char buffer[2048];
      size_t len = serializeJson(doc, buffer);
      client.publish(GATEWAY_TELEMETRY_TOPIC, (uint8_t*)buffer, len);
    }
    
    xSemaphoreGive(dataMutex);
  }
}

void gatewaySendAttributes(ThingsBoard& tb) {
  // Send attributes in batches (max ~20 nodes per batch for attributes)
  const uint8_t BATCH_SIZE = 20;
  
  auto& client = tb.getClient();
  
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    uint8_t batchCount = 0;
    StaticJsonDocument<2048> doc;
    char nodeName[16];
    bool anyNodeSent = false;
    
    for (uint8_t i = 0; i < MAX_NODES; i++) {
      if (nodesData[i].connected && nodesData[i].registered) {
        uint8_t nodeId = i + 1;
        getNodeName(nodeId, nodeName, sizeof(nodeName));
        
        JsonObject nodeAttr = doc.createNestedObject(nodeName);
        nodeAttr["temperatureThreshold"] = temperatureThreshold;
        nodeAttr["humidityThreshold"] = humidityThreshold;
        nodeAttr["soilMoistureThreshold"] = soilMoistureThreshold;
        nodeAttr["sendInterval"] = nodeSendInterval;
        
        batchCount++;
        anyNodeSent = true;
        
        // Send batch when limit reached
        if (batchCount >= BATCH_SIZE) {
          char buffer[2048];
          size_t len = serializeJson(doc, buffer);
          client.publish(GATEWAY_ATTRIBUTES_TOPIC, (uint8_t*)buffer, len);
          
          doc.clear();
          batchCount = 0;
        }
      }
    }
    
    // Send remaining nodes
    if (batchCount > 0 && anyNodeSent) {
      char buffer[2048];
      size_t len = serializeJson(doc, buffer);
      client.publish(GATEWAY_ATTRIBUTES_TOPIC, (uint8_t*)buffer, len);
      Serial.println("Gateway: Threshold attributes forwarded to active nodes");
    }
    
    xSemaphoreGive(dataMutex);
  }
}

void processSharedAttributes(const JsonObjectConst &data) {
  bool thresholdChanged = false;
  
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), TEMP_THRESHOLD_ATTR) == 0) {
      float newThreshold = it->value().as<float>();
      temperatureThreshold = newThreshold;
      Serial.print("Temperature threshold updated to: ");
      Serial.println(temperatureThreshold);
      thresholdChanged = true;
    } 
    else if (strcmp(it->key().c_str(), HUM_THRESHOLD_ATTR) == 0) {
      float newThreshold = it->value().as<float>();
      humidityThreshold = newThreshold;
      Serial.print("Humidity threshold updated to: ");
      Serial.println(humidityThreshold);
      thresholdChanged = true;
    }
    else if (strcmp(it->key().c_str(), SOIL_THRESHOLD_ATTR) == 0) {
      float newThreshold = it->value().as<float>();
      soilMoistureThreshold = newThreshold;
      Serial.print("Soil moisture threshold updated to: ");
      Serial.println(soilMoistureThreshold);
      thresholdChanged = true;
    }
    else if (strcmp(it->key().c_str(), SEND_INTERVAL_ATTR) == 0) {
      uint32_t newInterval = it->value().as<uint32_t>();
      nodeSendInterval = newInterval;
      Serial.print("Node send interval updated to: ");
      Serial.print(nodeSendInterval);
      Serial.println(" s");
      thresholdChanged = true;
    }
  }
  
  if (thresholdChanged) {
    // Set flag để ThingsBoard task forward attributes cho nodes
    // CHỈ forward cho nodes ĐÃ GỬI TELEMETRY (để alarm hoạt động)
    thresholdUpdatePending = true;
    Serial.println("Thresholds updated - will forward to active nodes and via LoRa ACK");
  }
}

void requestTimedOut() {
  Serial.println("Attribute request timed out");
}
