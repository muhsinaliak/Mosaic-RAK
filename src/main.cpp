/**
 * @file main.cpp
 * @brief Mosaic RAK Gateway - Main Firmware
 * @version 1.0.0
 *
 * ESP32-S3 based LoRa Gateway with:
 * - W5500 Ethernet
 * - WiFi connectivity
 * - AP Mode with Captive Portal
 * - MQTT communication
 * - NeoPixel status LED
 * - Factory reset button
 */

#include <Arduino.h>
#include "config.h"
#include "led_controller.h"
#include "config_manager.h"
#include "network_manager.h"
#include "mqtt_client.h"
#include "lora_manager.h"
#include "web_server.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// System state
static SystemStatus_t   systemStatus = SYS_STATUS_BOOT;
static uint32_t         bootTime = 0;

// Factory reset button
static uint32_t         buttonPressStart = 0;
static bool             buttonPressed = false;
static bool             factoryResetTriggered = false;

// Task timers
static uint32_t         lastHeartbeat = 0;
static uint32_t         lastStatusLog = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void initHardware(void);
void handleButton(void);
void performFactoryReset(void);
void printSystemInfo(void);
void onNetworkStatusChange(NetworkStatus_t status, IPAddress ip);
void onMQTTConnection(bool connected);
void onMQTTMessage(const String& topic, const JsonDocument& payload);
void onNodeData(uint8_t nodeID, const DataPacket_t& data);
void onNodeDiscovered(const DiscoveredNode& node);
void onPairingComplete(uint8_t nodeID, bool success);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize serial
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================================");
    Serial.println("     Mosaic RAK Gateway - Mintyfi LoRa");
    Serial.println("================================================");
    Serial.printf("Firmware Version: %s\n", GATEWAY_VERSION);
    Serial.printf("Build Date: %s %s\n", __DATE__, __TIME__);
    Serial.println("================================================");
    Serial.println();

    bootTime = millis();

    // Initialize hardware
    initHardware();

    // Initialize LED controller
    statusLED.begin();
    statusLED.setStatus(SYS_STATUS_BOOT);

    // Initialize configuration manager
    if (!configManager.begin()) {
        LOG_ERROR("MAIN", "Failed to initialize config manager!");
        statusLED.setStatus(SYS_STATUS_ERROR);
        while (1) {
            statusLED.update();
            delay(10);
        }
    }

    // Print system info
    printSystemInfo();

    // Initialize network manager
    networkManager.onStatusChange(onNetworkStatusChange);
    if (!networkManager.begin()) {
        LOG_ERROR("MAIN", "Failed to initialize network manager!");
    }

    // Initialize MQTT client
    mqttClient.onConnection(onMQTTConnection);
    mqttClient.onMessage(onMQTTMessage);

    String mqttServer = configManager.getMQTTServer();
    if (mqttServer.length() > 0) {
        mqttClient.begin(mqttServer, configManager.getMQTTPort());
        mqttClient.setAuth(configManager.getMQTTUser(), configManager.getMQTTPassword());
    }

    // Initialize Serial2 for RAK3172 communication
    LOG_INFO("MAIN", "Initializing LoRa Serial...");
    Serial2.begin(LORA_UART_BAUD, SERIAL_8N1, LORA_UART_RX, LORA_UART_TX);
    delay(100);

    // Initialize LoRa manager
    loraManager.onNodeData(onNodeData);
    loraManager.onNodeDiscovered(onNodeDiscovered);
    loraManager.onPairingComplete(onPairingComplete);

    if (!loraManager.begin(Serial2)) {
        LOG_ERROR("MAIN", "Failed to initialize LoRa manager!");
    } else {
        LOG_INFO("MAIN", "LoRa manager initialized");
    }

    // Load saved nodes from file system
    if (loraManager.loadNodes()) {
        LOG_INFO("MAIN", "Loaded registered nodes from storage");
        Serial.printf("[LORA] Registered nodes: %d\n", loraManager.getRegisteredNodeCount());
    }

    // Initialize Web Server (after network is ready)
    if (!webServerManager.begin()) {
        LOG_ERROR("MAIN", "Failed to start web server!");
    } else {
        LOG_INFO("MAIN", "Web server started");
    }

    LOG_INFO("MAIN", "Setup complete!");
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t now = millis();

    // Handle factory reset button
    handleButton();

    // If factory reset triggered, don't do anything else
    if (factoryResetTriggered) {
        statusLED.update();
        return;
    }

    // Update modules
    statusLED.update();
    networkManager.update();
    loraManager.update();
    webServerManager.update();

    // Update MQTT if network is connected
    if (networkManager.isConnected()) {
        mqttClient.update();
    }

    // Periodic status log
    if (now - lastStatusLog >= 30000) {  // Every 30 seconds
        lastStatusLog = now;

        Serial.printf("[STATUS] Uptime: %lu s, Heap: %lu/%lu KB, Network: %s, MQTT: %s, Nodes: %d/%d\n",
            (now - bootTime) / 1000,
            ESP.getFreeHeap() / 1024,
            ESP.getHeapSize() / 1024,
            networkManager.getConnectionType().c_str(),
            mqttClient.isConnected() ? "Connected" : "Disconnected",
            loraManager.getOnlineNodeCount(),
            loraManager.getRegisteredNodeCount()
        );
    }

    // Small delay to prevent watchdog issues
    delay(1);
}

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

void initHardware() {
    LOG_INFO("MAIN", "Initializing hardware...");

    // Configure button pin
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Configure NeoPixel pin (handled by LED controller)

    LOG_INFO("MAIN", "Hardware initialized");
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void handleButton() {
    bool currentState = (digitalRead(BUTTON_PIN) == LOW);

    if (currentState && !buttonPressed) {
        // Button just pressed
        buttonPressed = true;
        buttonPressStart = millis();
        LOG_INFO("MAIN", "Button pressed");
    } else if (!currentState && buttonPressed) {
        // Button released
        buttonPressed = false;
        uint32_t pressDuration = millis() - buttonPressStart;

        if (pressDuration < FACTORY_RESET_HOLD_TIME) {
            // Short press - could be used for other functions
            LOG_INFO("MAIN", "Short press detected");
        }
    } else if (currentState && buttonPressed) {
        // Button still held
        uint32_t pressDuration = millis() - buttonPressStart;

        // Visual feedback during hold
        if (pressDuration >= 3000 && pressDuration < FACTORY_RESET_HOLD_TIME) {
            // Warn user with orange breathing
            if (systemStatus != SYS_STATUS_FACTORY_RESET) {
                systemStatus = SYS_STATUS_FACTORY_RESET;
                statusLED.setStatus(SYS_STATUS_FACTORY_RESET);
                LOG_WARN("MAIN", "Hold for factory reset...");
            }
        }

        // Factory reset triggered
        if (pressDuration >= FACTORY_RESET_HOLD_TIME && !factoryResetTriggered) {
            factoryResetTriggered = true;
            performFactoryReset();
        }
    }
}

// ============================================================================
// FACTORY RESET
// ============================================================================

void performFactoryReset() {
    LOG_WARN("MAIN", "========================================");
    LOG_WARN("MAIN", "         FACTORY RESET INITIATED        ");
    LOG_WARN("MAIN", "========================================");

    // Visual feedback
    statusLED.setColor(LED_COLOR_RED, 100);

    // Disconnect MQTT
    if (mqttClient.isConnected()) {
        mqttClient.disconnect();
    }

    // Stop network
    networkManager.stopAPMode();

    delay(500);

    // Reset config to defaults (preserves SPA files)
    LOG_INFO("MAIN", "Resetting configuration...");
    if (configManager.format()) {
        LOG_INFO("MAIN", "Configuration reset successfully");
    } else {
        LOG_ERROR("MAIN", "Failed to reset configuration!");
    }

    // Blink to confirm
    for (int i = 0; i < 5; i++) {
        statusLED.setColor(LED_COLOR_RED, 100);
        delay(200);
        statusLED.off();
        delay(200);
    }

    LOG_INFO("MAIN", "Restarting...");
    delay(500);

    ESP.restart();
}

// ============================================================================
// SYSTEM INFO
// ============================================================================

void printSystemInfo() {
    Serial.println("--- System Information ---");
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("PSRAM Size: %d MB\n", ESP.getPsramSize() / (1024 * 1024));
    Serial.printf("Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("MAC Address: %s\n", networkManager.getMACString().c_str());
    Serial.printf("Device ID: %s\n", networkManager.getDeviceID().c_str());
    Serial.printf("LittleFS: %d/%d KB used\n",
        configManager.getUsedSpace() / 1024,
        configManager.getTotalSpace() / 1024);
    Serial.println("--------------------------");
    Serial.println();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void onNetworkStatusChange(NetworkStatus_t status, IPAddress ip) {
    switch (status) {
        case NET_ETHERNET_CONNECTED:
            LOG_INFO("MAIN", "Ethernet connected");
            Serial.printf("IP Address: %s\n", ip.toString().c_str());
            systemStatus = SYS_STATUS_ONLINE;

            // Try MQTT connection
            if (configManager.getMQTTServer().length() > 0) {
                mqttClient.connect();
            }
            break;

        case NET_WIFI_CONNECTED:
            LOG_INFO("MAIN", "WiFi connected");
            Serial.printf("IP Address: %s\n", ip.toString().c_str());
            Serial.printf("RSSI: %d dBm\n", networkManager.getRSSI());
            systemStatus = SYS_STATUS_ONLINE;

            // Try MQTT connection
            if (configManager.getMQTTServer().length() > 0) {
                mqttClient.connect();
            }
            break;

        case NET_AP_MODE:
            LOG_INFO("MAIN", "AP Mode active");
            Serial.printf("AP IP: %s\n", ip.toString().c_str());
            systemStatus = SYS_STATUS_AP_MODE;
            break;

        case NET_DISCONNECTED:
            LOG_WARN("MAIN", "Network disconnected");
            systemStatus = SYS_STATUS_OFFLINE;
            break;
    }
}

void onMQTTConnection(bool connected) {
    if (connected) {
        LOG_INFO("MAIN", "MQTT connected");
        statusLED.setStatus(SYS_STATUS_ONLINE);
    } else {
        LOG_WARN("MAIN", "MQTT disconnected");
        if (networkManager.isConnected()) {
            statusLED.setStatus(SYS_STATUS_MQTT_CONNECTING);
        }
    }
}

void onMQTTMessage(const String& topic, const JsonDocument& payload) {
    DEBUG_PRINTF("[MAIN] MQTT Message: %s\n", topic.c_str());

    // Handle node-related commands
    // Topic format: mintyfi/gateway/{deviceID}/nodes/{nodeID}/cmd
    if (topic.indexOf("/nodes/") >= 0 && topic.endsWith("/cmd")) {
        // Extract node ID from topic
        int nodesIdx = topic.indexOf("/nodes/");
        int cmdIdx = topic.lastIndexOf("/cmd");
        if (nodesIdx >= 0 && cmdIdx > nodesIdx) {
            String nodeIDStr = topic.substring(nodesIdx + 7, cmdIdx);
            uint8_t nodeID = nodeIDStr.toInt();

            // Process command
            if (payload.containsKey("relay")) {
                uint8_t relayBitmap = payload["relay"];
                loraManager.sendRelayCommand(nodeID, relayBitmap);
                LOG_INFO("MAIN", "MQTT: Relay command sent");
            } else if (payload.containsKey("toggle")) {
                uint8_t relayNum = payload["toggle"];
                loraManager.sendRelayToggle(nodeID, relayNum);
                LOG_INFO("MAIN", "MQTT: Toggle command sent");
            } else if (payload.containsKey("relay1") || payload.containsKey("relay2") || payload.containsKey("relay3") || payload.containsKey("relay4")) {
                // Handle individual relay control
                RegisteredNode* node = loraManager.getNodeByID(nodeID);
                if (node) {
                    uint8_t newStatus = node->relayStatus;

                    if (payload.containsKey("relay1")) {
                        if (payload["relay1"]) newStatus |= RELAY_1_BIT;
                        else newStatus &= ~RELAY_1_BIT;
                    }
                    if (payload.containsKey("relay2")) {
                        if (payload["relay2"]) newStatus |= RELAY_2_BIT;
                        else newStatus &= ~RELAY_2_BIT;
                    }
                    if (payload.containsKey("relay3")) {
                        if (payload["relay3"]) newStatus |= RELAY_3_BIT;
                        else newStatus &= ~RELAY_3_BIT;
                    }
                    if (payload.containsKey("relay4")) {
                        if (payload["relay4"]) newStatus |= RELAY_4_BIT;
                        else newStatus &= ~RELAY_4_BIT;
                    }

                    loraManager.sendRelayCommand(nodeID, newStatus);
                    LOG_INFO("MAIN", "MQTT: Individual relay command sent");
                }
            } else if (payload.containsKey("action")) {
                String action = payload["action"].as<String>();
                if (action == "status") {
                    loraManager.requestNodeStatus(nodeID);
                } else if (action == "reset") {
                    loraManager.sendResetCommand(nodeID);
                }
            }
        }
    }
}

// ============================================================================
// LORA CALLBACKS
// ============================================================================

void onNodeData(uint8_t nodeID, const DataPacket_t& data) {
    DEBUG_PRINTF("[LORA] Node %d data: relay=%02X\n", nodeID, data.relayStatus);

    // Publish to MQTT if connected
    if (mqttClient.isConnected()) {
        String topic = String(MQTT_TOPIC_PREFIX) + networkManager.getDeviceID() + "/nodes/" + String(nodeID) + "/status";

        JsonDocument doc;
        doc["node_id"] = nodeID;
        doc["relay_status"] = data.relayStatus;
        doc["relay_1"] = (data.relayStatus & RELAY_1_BIT) != 0;
        doc["relay_2"] = (data.relayStatus & RELAY_2_BIT) != 0;
        doc["uptime"] = data.uptime;
        doc["timestamp"] = millis() / 1000;

        String payload;
        serializeJson(doc, payload);
        mqttClient.publish(topic, payload, false);
    }
}

void onNodeDiscovered(const DiscoveredNode& node) {
    Serial.printf("[LORA] Discovered: MAC=%s, Type=%d, RSSI=%d dBm\n",
        LoRaManager::macToString(node.macAddr).c_str(),
        node.deviceType,
        node.rssi
    );

    // Visual feedback
    statusLED.blink(LED_COLOR_BLUE, 1, 200, 200);
}

void onPairingComplete(uint8_t nodeID, bool success) {
    if (success) {
        Serial.printf("[LORA] Pairing successful! Node ID: %d\n", nodeID);
        statusLED.blink(LED_COLOR_GREEN, 3, 200, 200);

        // Save nodes to file system
        if (loraManager.saveNodes()) {
            LOG_INFO("MAIN", "Nodes saved to storage");
        }

        // Publish to MQTT
        if (mqttClient.isConnected()) {
            String topic = String(MQTT_TOPIC_PREFIX) + networkManager.getDeviceID() + "/events";

            JsonDocument doc;
            doc["event"] = "node_paired";
            doc["node_id"] = nodeID;
            doc["timestamp"] = millis() / 1000;

            String payload;
            serializeJson(doc, payload);
            mqttClient.publish(topic, payload, false);
        }
    } else {
        Serial.println("[LORA] Pairing failed!");
        statusLED.blink(LED_COLOR_RED, 3, 200, 200);
    }
}
