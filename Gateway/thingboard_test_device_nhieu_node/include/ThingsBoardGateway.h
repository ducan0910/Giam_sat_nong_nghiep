#ifndef THINGSBOARD_GATEWAY_H
#define THINGSBOARD_GATEWAY_H

#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>
#include "LoRaNodeData.h"

// Gateway API Topics
extern const char GATEWAY_TELEMETRY_TOPIC[];
extern const char GATEWAY_ATTRIBUTES_TOPIC[];
extern const char GATEWAY_CONNECT_TOPIC[];
extern const char GATEWAY_DISCONNECT_TOPIC[];
extern const char GATEWAY_RPC_TOPIC[];

// LoRa Node configuration
extern const char DEVICE_TYPE[];

// Generate node name dynamically: "LoRaNode1", "LoRaNode2", ..., "LoRaNode100"
void getNodeName(uint8_t nodeId, char* buffer, size_t bufferSize);

// Attribute names
extern const char TEMP_THRESHOLD_ATTR[];
extern const char HUM_THRESHOLD_ATTR[];
extern const char SOIL_THRESHOLD_ATTR[];
extern const char SEND_INTERVAL_ATTR[];

// Flag to indicate threshold update needs to be forwarded to nodes
extern volatile bool thresholdUpdatePending;

/**
 * @brief Send Gateway Connect message for a LoRa node
 * @param tb ThingsBoard instance
 * @param nodeName Name of the node to connect
 */
void gatewayConnectNode(ThingsBoard& tb, const char* nodeName);

/**
 * @brief Send Gateway Disconnect message for a LoRa node
 * @param tb ThingsBoard instance
 * @param nodeName Name of the node to disconnect
 */
void gatewayDisconnectNode(ThingsBoard& tb, const char* nodeName);

/**
 * @brief Send Gateway Telemetry for multiple nodes
 * @param tb ThingsBoard instance
 */
void gatewaySendTelemetry(ThingsBoard& tb);

/**
 * @brief Send Gateway Attributes for multiple nodes
 * @param tb ThingsBoard instance
 */
void gatewaySendAttributes(ThingsBoard& tb);

/**
 * @brief Process shared attributes updates from ThingsBoard
 * @param data JSON object containing attribute updates
 */
void processSharedAttributes(const JsonObjectConst &data);

/**
 * @brief Callback when attribute request times out
 */
void requestTimedOut();

#endif // THINGSBOARD_GATEWAY_H
