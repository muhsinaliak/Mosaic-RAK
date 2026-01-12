/**
 * @file lora_manager.h
 * @brief LoRa P2P Manager - RAK3172 Communication Handler
 * @version 1.0.0
 *
 * RAK3172 modülü ile UART üzerinden haberleşme.
 * Binary protokol ile Node keşfi ve yönetimi.
 */

#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include "config.h"
#include "protocol.h"

// ============================================================================
// NODE VERI YAPILARI
// ============================================================================

/**
 * @brief Keşif sırasında bulunan cihaz bilgisi
 */
struct DiscoveredNode {
    uint8_t     macAddr[MAC_ADDR_LEN];
    uint8_t     deviceType;
    uint8_t     fwVersion;
    int8_t      rssi;
    int8_t      snr;
    uint32_t    discoveredAt;   // millis() timestamp
    bool        valid;
};

/**
 * @brief Kayıtlı (eşleşmiş) node bilgisi
 */
struct RegisteredNode {
    uint8_t     nodeID;
    uint8_t     macAddr[MAC_ADDR_LEN];
    uint8_t     deviceType;
    char        name[32];
    uint8_t     relayStatus;
    int8_t      lastRSSI;
    int8_t      lastSNR;
    uint32_t    uptime;
    uint32_t    lastSeen;       // millis() timestamp
    bool        online;
    bool        valid;
};

/**
 * @brief Eşleşme durumu
 */
typedef enum {
    PAIR_IDLE,
    PAIR_WAITING_ACK,
    PAIR_SUCCESS,
    PAIR_TIMEOUT,
    PAIR_FAILED
} PairingState_t;

// ============================================================================
// CALLBACK TİPLERİ
// ============================================================================

typedef std::function<void(uint8_t nodeID, const DataPacket_t& data)> NodeDataCallback;
typedef std::function<void(const DiscoveredNode& node)> NodeDiscoveredCallback;
typedef std::function<void(uint8_t nodeID, bool success)> PairingCallback;

// ============================================================================
// LORA MANAGER SINIFI
// ============================================================================

class LoRaManager {
public:
    /**
     * @brief Constructor
     */
    LoRaManager();

    /**
     * @brief Initialize LoRa module
     * @param serial HardwareSerial instance (Serial2)
     * @return true if successful
     */
    bool begin(HardwareSerial& serial);

    /**
     * @brief Non-blocking update - call frequently in loop()
     */
    void update();

    // ========== SCAN (KEŞİF) MODU ==========

    /**
     * @brief Start scan mode for discovering new nodes
     * @param duration Duration in milliseconds (default 60s)
     */
    void startScan(uint32_t duration = LORA_SCAN_DURATION);

    /**
     * @brief Stop scan mode
     */
    void stopScan();

    /**
     * @brief Check if scan mode is active
     */
    bool isScanning() const { return _scanMode; }

    /**
     * @brief Get discovered nodes list
     */
    const std::vector<DiscoveredNode>& getDiscoveredNodes() const { return _discoveredNodes; }

    /**
     * @brief Clear discovered nodes list
     */
    void clearDiscoveredNodes();

    // ========== EŞLEŞTİRME (PAIRING) ==========

    /**
     * @brief Start pairing with a discovered node
     * @param macAddr MAC address of the node to pair
     * @return true if pairing started
     */
    bool startPairing(const uint8_t* macAddr);

    /**
     * @brief Get pairing state
     */
    PairingState_t getPairingState() const { return _pairingState; }

    /**
     * @brief Cancel ongoing pairing
     */
    void cancelPairing();

    // ========== KAYITLI NODE YÖNETİMİ ==========

    /**
     * @brief Get registered nodes list
     */
    const std::vector<RegisteredNode>& getRegisteredNodes() const { return _registeredNodes; }

    /**
     * @brief Get registered node by ID
     */
    RegisteredNode* getNodeByID(uint8_t nodeID);

    /**
     * @brief Get registered node by MAC
     */
    RegisteredNode* getNodeByMAC(const uint8_t* macAddr);

    /**
     * @brief Remove a registered node
     */
    bool removeNode(uint8_t nodeID);

    /**
     * @brief Get online node count
     */
    uint8_t getOnlineNodeCount() const;

    /**
     * @brief Get total registered node count
     */
    uint8_t getRegisteredNodeCount() const { return _registeredNodes.size(); }

    // ========== KOMUT GÖNDERME ==========

    /**
     * @brief Send relay control command
     * @param nodeID Target node ID
     * @param relayBitmap Relay state bitmap
     * @return true if command sent
     */
    bool sendRelayCommand(uint8_t nodeID, uint8_t relayBitmap);

    /**
     * @brief Toggle specific relay
     * @param nodeID Target node ID
     * @param relayNum Relay number (1-4)
     * @return true if command sent
     */
    bool sendRelayToggle(uint8_t nodeID, uint8_t relayNum);

    /**
     * @brief Request status from node
     */
    bool requestNodeStatus(uint8_t nodeID);

    /**
     * @brief Send reset command
     */
    bool sendResetCommand(uint8_t nodeID);

    // ========== CALLBACKS ==========

    void onNodeData(NodeDataCallback callback) { _nodeDataCallback = callback; }
    void onNodeDiscovered(NodeDiscoveredCallback callback) { _nodeDiscoveredCallback = callback; }
    void onPairingComplete(PairingCallback callback) { _pairingCallback = callback; }

    // ========== PERSISTENCE ==========

    /**
     * @brief Save registered nodes to LittleFS
     */
    bool saveNodes();

    /**
     * @brief Load registered nodes from LittleFS
     */
    bool loadNodes();

    // ========== UTILITY ==========

    /**
     * @brief Convert MAC address to string
     */
    static String macToString(const uint8_t* mac);

    /**
     * @brief Parse MAC string to bytes
     */
    static bool stringToMAC(const String& str, uint8_t* mac);

private:
    HardwareSerial* _serial;
    bool            _initialized;

    // RX Buffer
    uint8_t         _rxBuffer[256];
    uint16_t        _rxIndex;
    uint32_t        _lastRxTime;

    // Scan mode
    bool            _scanMode;
    uint32_t        _scanStartTime;
    uint32_t        _scanDuration;
    std::vector<DiscoveredNode> _discoveredNodes;

    // Pairing
    PairingState_t  _pairingState;
    uint8_t         _pairingMAC[MAC_ADDR_LEN];
    uint8_t         _pairingNodeID;
    uint32_t        _pairingStartTime;

    // Registered nodes
    std::vector<RegisteredNode> _registeredNodes;

    // Callbacks
    NodeDataCallback        _nodeDataCallback;
    NodeDiscoveredCallback  _nodeDiscoveredCallback;
    PairingCallback         _pairingCallback;

    // Timers
    uint32_t        _lastNodeCheck;

    // Private methods
    void            _processRxData();
    void            _handlePacket(const uint8_t* data, uint16_t len);
    void            _handleHello(const HelloPacket_t* pkt, int8_t rssi, int8_t snr);
    void            _handleAck(const AckPacket_t* pkt);
    void            _handleData(const DataPacket_t* pkt);
    void            _handleHeartbeat(const HeartbeatPacket_t* pkt);

    bool            _sendPacket(const uint8_t* data, uint16_t len);
    bool            _sendWelcome(const uint8_t* mac, uint8_t nodeID);
    bool            _sendCommand(uint8_t nodeID, uint8_t cmdType, uint8_t param1, uint8_t param2);

    uint8_t         _getNextFreeID();
    bool            _isNodeRegistered(const uint8_t* mac);
    bool            _isDuplicateDiscovered(const uint8_t* mac);
    void            _updateNodeOnlineStatus();
    void            _addDiscoveredNode(const HelloPacket_t* pkt, int8_t rssi, int8_t snr);

    int8_t          _parseRSSI(const String& response);
    int8_t          _parseSNR(const String& response);
};

// Global instance
extern LoRaManager loraManager;

#endif // LORA_MANAGER_H
