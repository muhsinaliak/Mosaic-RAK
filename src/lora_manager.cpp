/**
 * @file lora_manager.cpp
 * @brief LoRa P2P Manager Implementation
 * @version 1.0.0
 *
 * RAK3172 modülü ile haberleşme:
 * - AT komutları ile konfigürasyon
 * - Binary paketler ile veri alışverişi
 * - Non-blocking tasarım
 */

#include "lora_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Global instance
LoRaManager loraManager;

// AT Command responses
static const char* AT_OK = "OK";
static const char* AT_ERROR = "ERROR";

// Node offline threshold (ms)
static const uint32_t NODE_OFFLINE_THRESHOLD = 120000;  // 2 dakika

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

LoRaManager::LoRaManager()
    : _serial(nullptr)
    , _initialized(false)
    , _rxIndex(0)
    , _lastRxTime(0)
    , _scanMode(false)
    , _scanStartTime(0)
    , _scanDuration(LORA_SCAN_DURATION)
    , _pairingState(PAIR_IDLE)
    , _pairingNodeID(0)
    , _pairingStartTime(0)
    , _nodeDataCallback(nullptr)
    , _nodeDiscoveredCallback(nullptr)
    , _pairingCallback(nullptr)
    , _lastNodeCheck(0)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(_pairingMAC, 0, MAC_ADDR_LEN);
}

bool LoRaManager::begin(HardwareSerial& serial) {
    _serial = &serial;

    LOG_INFO("LORA", "Initializing RAK3172...");

    // Configure serial
    _serial->begin(LORA_UART_BAUD, SERIAL_8N1, LORA_UART_RX, LORA_UART_TX);

    // Wait for module
    uint32_t startTime = millis();
    while (!_serial->available() && (millis() - startTime < 2000)) {
        _serial->println("AT");
        uint32_t waitStart = millis();
        while (millis() - waitStart < 200) {
            if (_serial->available()) break;
        }
    }

    // Flush buffer
    while (_serial->available()) {
        _serial->read();
    }

    // Configure P2P mode
    _serial->println("AT+NWM=0");  // P2P mode
    uint32_t cmdStart = millis();
    while (millis() - cmdStart < 500) {
        if (_serial->available()) {
            String resp = _serial->readStringUntil('\n');
            if (resp.indexOf(AT_OK) >= 0 || resp.indexOf("0") >= 0) break;
        }
    }

    // Small delay for mode switch
    uint32_t delayStart = millis();
    while (millis() - delayStart < 300) { /* non-blocking delay */ }

    // Configure P2P parameters
    char cmd[64];

    // Frequency
    snprintf(cmd, sizeof(cmd), "AT+PFREQ=%lu", (unsigned long)LORA_FREQUENCY);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Spreading Factor
    snprintf(cmd, sizeof(cmd), "AT+PSF=%d", LORA_SF);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Bandwidth
    snprintf(cmd, sizeof(cmd), "AT+PBW=%d", LORA_BW);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Coding Rate
    snprintf(cmd, sizeof(cmd), "AT+PCR=%d", LORA_CR);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // TX Power
    snprintf(cmd, sizeof(cmd), "AT+PTP=%d", LORA_TX_POWER);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Preamble
    snprintf(cmd, sizeof(cmd), "AT+PPL=%d", LORA_PREAMBLE);
    _serial->println(cmd);
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Enable continuous RX
    _serial->println("AT+PRECV=65534");
    delayStart = millis();
    while (millis() - delayStart < 100) { if (_serial->available()) _serial->read(); }

    // Load saved nodes
    loadNodes();

    _initialized = true;
    LOG_INFO("LORA", "RAK3172 initialized");
    Serial.printf("[LORA] Frequency: %lu Hz, SF: %d\n", (unsigned long)LORA_FREQUENCY, LORA_SF);

    return true;
}

// ============================================================================
// MAIN UPDATE LOOP (NON-BLOCKING)
// ============================================================================

void LoRaManager::update() {
    if (!_initialized || !_serial) return;

    uint32_t now = millis();

    // Process incoming data
    _processRxData();

    // Check scan timeout
    if (_scanMode && (now - _scanStartTime >= _scanDuration)) {
        LOG_INFO("LORA", "Scan completed");
        stopScan();
    }

    // Check pairing timeout
    if (_pairingState == PAIR_WAITING_ACK) {
        if (now - _pairingStartTime >= LORA_PAIRING_TIMEOUT) {
            LOG_WARN("LORA", "Pairing timeout");
            _pairingState = PAIR_TIMEOUT;
            if (_pairingCallback) {
                _pairingCallback(_pairingNodeID, false);
            }
            _pairingState = PAIR_IDLE;
        }
    }

    // Periodic node status check
    if (now - _lastNodeCheck >= 5000) {
        _lastNodeCheck = now;
        _updateNodeOnlineStatus();
    }
}

// ============================================================================
// RX DATA PROCESSING
// ============================================================================

void LoRaManager::_processRxData() {
    while (_serial->available()) {
        char c = _serial->read();
        _lastRxTime = millis();

        // Handle line-based responses
        if (c == '\n' || c == '\r') {
            if (_rxIndex > 0) {
                _rxBuffer[_rxIndex] = '\0';
                String line = String((char*)_rxBuffer);

                // Check for RX data notification: +EVT:RXP2P:RSSI:SNR:DATA
                if (line.startsWith("+EVT:RXP2P:")) {
                    // Parse: +EVT:RXP2P:-45:8:0102030405...
                    int firstColon = line.indexOf(':', 11);
                    int secondColon = line.indexOf(':', firstColon + 1);

                    if (firstColon > 0 && secondColon > 0) {
                        int8_t rssi = line.substring(11, firstColon).toInt();
                        int8_t snr = line.substring(firstColon + 1, secondColon).toInt();
                        String hexData = line.substring(secondColon + 1);
                        hexData.trim();

                        // Convert hex string to bytes
                        uint8_t packetData[MAX_PACKET_SIZE];
                        uint16_t packetLen = 0;

                        for (unsigned int i = 0; i < hexData.length() && packetLen < MAX_PACKET_SIZE; i += 2) {
                            String byteStr = hexData.substring(i, i + 2);
                            packetData[packetLen++] = strtol(byteStr.c_str(), NULL, 16);
                        }

                        if (packetLen > 0) {
                            // Store RSSI/SNR for packet handling
                            // (simplified - in real impl pass to handler)
                            _handlePacket(packetData, packetLen);
                        }
                    }
                }

                _rxIndex = 0;
            }
        } else {
            if (_rxIndex < sizeof(_rxBuffer) - 1) {
                _rxBuffer[_rxIndex++] = c;
            }
        }
    }

    // Reset buffer on timeout
    if (_rxIndex > 0 && (millis() - _lastRxTime > 100)) {
        _rxIndex = 0;
    }
}

void LoRaManager::_handlePacket(const uint8_t* data, uint16_t len) {
    if (len < 1) return;

    uint8_t pktType = data[0];

    DEBUG_PRINTF("[LORA] RX Packet: type=0x%02X, len=%d\n", pktType, len);

    switch (pktType) {
        case PKG_HELLO:
            if (len >= sizeof(HelloPacket_t)) {
                _handleHello((HelloPacket_t*)data, -50, 10);  // TODO: pass real RSSI/SNR
            }
            break;

        case PKG_ACK:
            if (len >= sizeof(AckPacket_t)) {
                _handleAck((AckPacket_t*)data);
            }
            break;

        case PKG_DATA:
            if (len >= sizeof(DataPacket_t)) {
                _handleData((DataPacket_t*)data);
            }
            break;

        case PKG_HEARTBEAT:
            if (len >= sizeof(HeartbeatPacket_t)) {
                _handleHeartbeat((HeartbeatPacket_t*)data);
            }
            break;

        default:
            DEBUG_PRINTF("[LORA] Unknown packet type: 0x%02X\n", pktType);
            break;
    }
}

// ============================================================================
// PACKET HANDLERS
// ============================================================================

void LoRaManager::_handleHello(const HelloPacket_t* pkt, int8_t rssi, int8_t snr) {
    String macStr = macToString(pkt->macAddr);
    Serial.printf("[LORA] HELLO from %s, type=%d, fw=0x%02X\n",
                  macStr.c_str(), pkt->deviceType, pkt->fwVersion);

    // Only process in scan mode
    if (!_scanMode) {
        DEBUG_PRINTLN("[LORA] Not in scan mode, ignoring HELLO");
        return;
    }

    // Check if already discovered
    if (_isDuplicateDiscovered(pkt->macAddr)) {
        DEBUG_PRINTLN("[LORA] Duplicate HELLO, ignoring");
        return;
    }

    // Check if already registered
    if (_isNodeRegistered(pkt->macAddr)) {
        DEBUG_PRINTLN("[LORA] Node already registered");
        return;
    }

    // Add to discovered list
    _addDiscoveredNode(pkt, rssi, snr);
}

void LoRaManager::_handleAck(const AckPacket_t* pkt) {
    Serial.printf("[LORA] ACK from node %d, ackType=0x%02X, status=%d\n",
                  pkt->nodeID, pkt->ackType, pkt->status);

    // Check if we're waiting for pairing ACK
    if (_pairingState == PAIR_WAITING_ACK && pkt->ackType == PKG_WELCOME) {
        if (pkt->status == ERR_NONE) {
            LOG_INFO("LORA", "Pairing successful!");

            // Create new registered node
            RegisteredNode node;
            node.nodeID = _pairingNodeID;
            memcpy(node.macAddr, _pairingMAC, MAC_ADDR_LEN);
            
            // Cihaz tipini keşfedilenler listesinden bulmaya çalış
            node.deviceType = DEV_TYPE_RELAY_2CH; // Varsayılan
            for (const auto& dNode : _discoveredNodes) {
                if (memcmp(dNode.macAddr, _pairingMAC, MAC_ADDR_LEN) == 0) {
                    node.deviceType = dNode.deviceType;
                    break;
                }
            }

            snprintf(node.name, sizeof(node.name), "Node_%d", _pairingNodeID);
            node.relayStatus = 0;
            node.lastRSSI = 0;
            node.lastSNR = 0;
            node.uptime = 0;
            node.lastSeen = millis();
            node.online = true;
            node.valid = true;

            _registeredNodes.push_back(node);
            saveNodes();

            _pairingState = PAIR_SUCCESS;
            if (_pairingCallback) {
                _pairingCallback(_pairingNodeID, true);
            }
        } else {
            LOG_WARN("LORA", "Pairing failed - node error");
            _pairingState = PAIR_FAILED;
            if (_pairingCallback) {
                _pairingCallback(_pairingNodeID, false);
            }
        }
        _pairingState = PAIR_IDLE;
    }
}

void LoRaManager::_handleData(const DataPacket_t* pkt) {
    Serial.printf("[LORA] DATA from node %d: relay=0x%02X, rssi=%d, snr=%d, uptime=%lu\n",
                  pkt->nodeID, pkt->relayStatus, pkt->rssi, pkt->snr, (unsigned long)pkt->uptime);

    // Find registered node
    RegisteredNode* node = getNodeByID(pkt->nodeID);
    if (!node) {
        DEBUG_PRINTLN("[LORA] DATA from unknown node, ignoring");
        return;
    }

    // Update node data
    node->relayStatus = pkt->relayStatus;
    node->lastRSSI = pkt->rssi;
    node->lastSNR = pkt->snr;
    node->uptime = pkt->uptime;
    node->lastSeen = millis();
    node->online = true;

    // Callback
    if (_nodeDataCallback) {
        _nodeDataCallback(pkt->nodeID, *pkt);
    }
}

void LoRaManager::_handleHeartbeat(const HeartbeatPacket_t* pkt) {
    DEBUG_PRINTF("[LORA] HEARTBEAT from node %d\n", pkt->nodeID);

    RegisteredNode* node = getNodeByID(pkt->nodeID);
    if (node) {
        node->relayStatus = pkt->relayStatus;
        node->lastSeen = millis();
        node->online = true;
    }
}

// ============================================================================
// SCAN (KEŞİF) MODU
// ============================================================================

void LoRaManager::startScan(uint32_t duration) {
    LOG_INFO("LORA", "Starting scan mode...");

    _scanMode = true;
    _scanStartTime = millis();
    _scanDuration = duration;
    clearDiscoveredNodes();

    Serial.printf("[LORA] Scan duration: %lu ms\n", duration);
}

void LoRaManager::stopScan() {
    _scanMode = false;
    LOG_INFO("LORA", "Scan stopped");
    Serial.printf("[LORA] Discovered %d nodes\n", _discoveredNodes.size());
}

void LoRaManager::clearDiscoveredNodes() {
    _discoveredNodes.clear();
}

void LoRaManager::_addDiscoveredNode(const HelloPacket_t* pkt, int8_t rssi, int8_t snr) {
    if (_discoveredNodes.size() >= MAX_DISCOVERED_NODES) {
        LOG_WARN("LORA", "Max discovered nodes reached");
        return;
    }

    DiscoveredNode node;
    memcpy(node.macAddr, pkt->macAddr, MAC_ADDR_LEN);
    node.deviceType = pkt->deviceType;
    node.fwVersion = pkt->fwVersion;
    node.rssi = rssi;
    node.snr = snr;
    node.discoveredAt = millis();
    node.valid = true;

    _discoveredNodes.push_back(node);

    String macStr = macToString(pkt->macAddr);
    Serial.printf("[LORA] New device discovered: %s\n", macStr.c_str());

    if (_nodeDiscoveredCallback) {
        _nodeDiscoveredCallback(node);
    }
}

bool LoRaManager::_isDuplicateDiscovered(const uint8_t* mac) {
    for (const auto& node : _discoveredNodes) {
        if (memcmp(node.macAddr, mac, MAC_ADDR_LEN) == 0) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// EŞLEŞTİRME (PAIRING)
// ============================================================================

bool LoRaManager::startPairing(const uint8_t* macAddr) {
    if (_pairingState != PAIR_IDLE) {
        LOG_WARN("LORA", "Pairing already in progress");
        return false;
    }

    // Check if already registered
    if (_isNodeRegistered(macAddr)) {
        LOG_WARN("LORA", "Node already registered");
        return false;
    }

    // Get next free ID
    _pairingNodeID = _getNextFreeID();
    if (_pairingNodeID == 0) {
        LOG_ERROR("LORA", "No free node IDs available");
        return false;
    }

    memcpy(_pairingMAC, macAddr, MAC_ADDR_LEN);
    _pairingStartTime = millis();
    _pairingState = PAIR_WAITING_ACK;

    String macStr = macToString(macAddr);
    Serial.printf("[LORA] Starting pairing with %s, assigning ID %d\n",
                  macStr.c_str(), _pairingNodeID);

    // Send WELCOME packet
    return _sendWelcome(macAddr, _pairingNodeID);
}

void LoRaManager::cancelPairing() {
    _pairingState = PAIR_IDLE;
    memset(_pairingMAC, 0, MAC_ADDR_LEN);
    _pairingNodeID = 0;
}

bool LoRaManager::_isNodeRegistered(const uint8_t* mac) {
    for (const auto& node : _registeredNodes) {
        if (node.valid && memcmp(node.macAddr, mac, MAC_ADDR_LEN) == 0) {
            return true;
        }
    }
    return false;
}

uint8_t LoRaManager::_getNextFreeID() {
    bool used[256] = {false};
    used[0] = true;     // Reserved
    used[255] = true;   // Broadcast

    for (const auto& node : _registeredNodes) {
        if (node.valid) {
            used[node.nodeID] = true;
        }
    }

    for (uint8_t id = 1; id < 255; id++) {
        if (!used[id]) {
            return id;
        }
    }

    return 0;  // No free ID
}

// ============================================================================
// KAYITLI NODE YÖNETİMİ
// ============================================================================

RegisteredNode* LoRaManager::getNodeByID(uint8_t nodeID) {
    for (auto& node : _registeredNodes) {
        if (node.valid && node.nodeID == nodeID) {
            return &node;
        }
    }
    return nullptr;
}

RegisteredNode* LoRaManager::getNodeByMAC(const uint8_t* macAddr) {
    for (auto& node : _registeredNodes) {
        if (node.valid && memcmp(node.macAddr, macAddr, MAC_ADDR_LEN) == 0) {
            return &node;
        }
    }
    return nullptr;
}

bool LoRaManager::removeNode(uint8_t nodeID) {
    for (auto it = _registeredNodes.begin(); it != _registeredNodes.end(); ++it) {
        if (it->nodeID == nodeID) {
            _registeredNodes.erase(it);
            saveNodes();
            Serial.printf("[LORA] Node %d removed\n", nodeID);
            return true;
        }
    }
    return false;
}

uint8_t LoRaManager::getOnlineNodeCount() const {
    uint8_t count = 0;
    for (const auto& node : _registeredNodes) {
        if (node.valid && node.online) {
            count++;
        }
    }
    return count;
}

void LoRaManager::_updateNodeOnlineStatus() {
    uint32_t now = millis();
    for (auto& node : _registeredNodes) {
        if (node.valid && node.online) {
            if (now - node.lastSeen > NODE_OFFLINE_THRESHOLD) {
                node.online = false;
                Serial.printf("[LORA] Node %d went offline\n", node.nodeID);
            }
        }
    }
}

// ============================================================================
// KOMUT GÖNDERME
// ============================================================================

bool LoRaManager::sendRelayCommand(uint8_t nodeID, uint8_t relayBitmap) {
    return _sendCommand(nodeID, CMD_RELAY_SET, relayBitmap, 0);
}

bool LoRaManager::sendRelayToggle(uint8_t nodeID, uint8_t relayNum) {
    return _sendCommand(nodeID, CMD_RELAY_TOGGLE, relayNum, 0);
}

bool LoRaManager::requestNodeStatus(uint8_t nodeID) {
    return _sendCommand(nodeID, CMD_REQUEST_STATUS, 0, 0);
}

bool LoRaManager::sendResetCommand(uint8_t nodeID) {
    return _sendCommand(nodeID, CMD_RESET, 0, 0);
}

bool LoRaManager::_sendCommand(uint8_t nodeID, uint8_t cmdType, uint8_t param1, uint8_t param2) {
    CommandPacket_t pkt;
    pkt.type = PKG_CMD;
    pkt.targetID = nodeID;
    pkt.cmdType = cmdType;
    pkt.param1 = param1;
    pkt.param2 = param2;

    Serial.printf("[LORA] Sending CMD to node %d: cmd=%d, p1=%d, p2=%d\n",
                  nodeID, cmdType, param1, param2);

    return _sendPacket((uint8_t*)&pkt, sizeof(pkt));
}

bool LoRaManager::_sendWelcome(const uint8_t* mac, uint8_t nodeID) {
    WelcomePacket_t pkt;
    pkt.type = PKG_WELCOME;
    memcpy(pkt.targetMac, mac, MAC_ADDR_LEN);
    pkt.assignedID = nodeID;
    pkt.reserved = 0;

    return _sendPacket((uint8_t*)&pkt, sizeof(pkt));
}

bool LoRaManager::_sendPacket(const uint8_t* data, uint16_t len) {
    if (!_serial || !_initialized) return false;

    // Clear buffer to avoid reading old responses
    while (_serial->available()) _serial->read();

    // Stop RX
    _serial->println("AT+PRECV=0");
    uint32_t delayStart = millis();
    // Wait for OK from PRECV=0 to ensure mode switch
    while (millis() - delayStart < 200) {
        if (_serial->available()) {
            String resp = _serial->readStringUntil('\n');
            if (resp.indexOf(AT_OK) >= 0) break;
        }
    }

    // Build hex string
    String hexStr = "";
    for (uint16_t i = 0; i < len; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", data[i]);
        hexStr += hex;
    }

    // Send
    String cmd = "AT+PSEND=" + hexStr;
    _serial->println(cmd);

    // Wait for OK
    delayStart = millis();
    bool success = false;
    while (millis() - delayStart < LORA_TX_TIMEOUT) {
        if (_serial->available()) {
            String resp = _serial->readStringUntil('\n');
            if (resp.indexOf(AT_OK) >= 0) {
                success = true;
                break;
            } else if (resp.indexOf(AT_ERROR) >= 0) {
                break;
            }
        }
    }

    // Give module a moment to process TX before switching back to RX
    delay(50);

    // Resume RX
    _serial->println("AT+PRECV=65534");

    return success;
}

// ============================================================================
// PERSISTENCE (LittleFS)
// ============================================================================

bool LoRaManager::saveNodes() {
    JsonDocument doc;
    JsonArray nodesArray = doc["nodes"].to<JsonArray>();

    for (const auto& node : _registeredNodes) {
        if (!node.valid) continue;

        JsonObject nodeObj = nodesArray.add<JsonObject>();
        nodeObj["id"] = node.nodeID;
        nodeObj["mac"] = macToString(node.macAddr);
        nodeObj["type"] = node.deviceType;
        nodeObj["name"] = node.name;
    }

    File file = LittleFS.open(NODES_FILE_PATH, "w");
    if (!file) {
        LOG_ERROR("LORA", "Failed to open nodes file for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    LOG_INFO("LORA", "Nodes saved to flash");
    return true;
}

bool LoRaManager::loadNodes() {
    if (!LittleFS.exists(NODES_FILE_PATH)) {
        LOG_INFO("LORA", "No saved nodes file");
        return false;
    }

    File file = LittleFS.open(NODES_FILE_PATH, "r");
    if (!file) {
        LOG_ERROR("LORA", "Failed to open nodes file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_ERROR("LORA", "Failed to parse nodes JSON");
        return false;
    }

    _registeredNodes.clear();

    JsonArray nodesArray = doc["nodes"].as<JsonArray>();
    for (JsonObject nodeObj : nodesArray) {
        RegisteredNode node;
        node.nodeID = nodeObj["id"];

        String macStr = nodeObj["mac"].as<String>();
        stringToMAC(macStr, node.macAddr);

        node.deviceType = nodeObj["type"];
        strlcpy(node.name, nodeObj["name"] | "Unknown", sizeof(node.name));
        node.relayStatus = 0;
        node.lastRSSI = 0;
        node.lastSNR = 0;
        node.uptime = 0;
        node.lastSeen = 0;
        node.online = false;
        node.valid = true;

        _registeredNodes.push_back(node);
    }

    Serial.printf("[LORA] Loaded %d nodes from flash\n", _registeredNodes.size());
    return true;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

String LoRaManager::macToString(const uint8_t* mac) {
    char str[18];
    snprintf(str, sizeof(str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(str);
}

bool LoRaManager::stringToMAC(const String& str, uint8_t* mac) {
    if (str.length() < 17) return false;

    int values[6];
    if (sscanf(str.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }

    return true;
}
