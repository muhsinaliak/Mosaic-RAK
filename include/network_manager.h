/**
 * @file network_manager.h
 * @brief Network Manager - Ethernet/WiFi/AP Mode Handler
 * @version 1.0.0
 *
 * Öncelik sırası:
 * 1. Ethernet (W5500)
 * 2. WiFi (kayıtlı ağ)
 * 3. AP Mode (Captive Portal)
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <functional>
#include "config.h"

// Callback tipleri
typedef std::function<void(NetworkStatus_t status, IPAddress ip)> NetworkCallback;

class NetworkManager {
public:
    /**
     * @brief Constructor
     */
    NetworkManager();

    /**
     * @brief Initialize network manager
     * @return true if initialization successful
     */
    bool begin();

    /**
     * @brief Main update loop - call frequently
     */
    void update();

    /**
     * @brief Get current network status
     */
    NetworkStatus_t getStatus() const { return _status; }

    /**
     * @brief Check if connected to any network
     */
    bool isConnected() const;

    /**
     * @brief Get current IP address
     */
    IPAddress getIP() const { return _currentIP; }

    /**
     * @brief Get MAC address as string
     */
    String getMACString() const { return _macString; }

    /**
     * @brief Get unique device ID (based on MAC)
     */
    String getDeviceID() const;

    /**
     * @brief Force AP mode
     */
    void startAPMode();

    /**
     * @brief Stop AP mode and try reconnecting
     */
    void stopAPMode();

    /**
     * @brief Disconnect and reconnect
     */
    void reconnect();

    /**
     * @brief Set network status callback
     */
    void onStatusChange(NetworkCallback callback);

    /**
     * @brief Save WiFi credentials
     */
    bool saveWiFiCredentials(const String& ssid, const String& password);

    /**
     * @brief Clear saved credentials
     */
    void clearCredentials();

    /**
     * @brief Check if Ethernet cable is connected
     */
    bool isEthernetCableConnected();

    /**
     * @brief Check if Ethernet has valid IP (even in AP mode)
     */
    bool isEthernetConnected() const;

    /**
     * @brief Get Ethernet IP address (even in AP mode)
     */
    IPAddress getEthernetIP() const;

    /**
     * @brief Get RSSI (WiFi only)
     */
    int32_t getRSSI() const;

    /**
     * @brief Get connection type as string
     */
    String getConnectionType() const;

    /**
     * @brief Initialize Ethernet hardware (for late initialization)
     */
    bool initEthernet();

    /**
     * @brief Check if Ethernet hardware is initialized
     */
    bool isEthernetInitialized() const { return _ethernetInitialized; }

private:
    NetworkStatus_t     _status;
    IPAddress           _currentIP;
    String              _macString;
    uint8_t             _mac[6];

    // Timing
    uint32_t            _lastConnectAttempt;
    uint32_t            _connectStartTime;
    uint32_t            _lastStatusCheck;

    // Flags
    bool                _ethernetInitialized;
    bool                _wifiInitialized;
    bool                _apModeActive;
    bool                _captivePortalActive;

    // Saved credentials
    String              _savedSSID;
    String              _savedPassword;

    // Callback
    NetworkCallback     _statusCallback;

    // Private methods
    bool                _initEthernet();
    bool                _initWiFi();
    bool                _tryEthernetConnect();
    bool                _tryWiFiConnect();
    void                _startCaptivePortal();
    void                _stopCaptivePortal();
    void                _handleCaptivePortal();
    void                _updateStatus(NetworkStatus_t newStatus);
    void                _loadCredentials();
    void                _generateMAC();
    String              _generateAPSSID();
};

// Global instance
extern NetworkManager networkManager;

#endif // NETWORK_MANAGER_H
