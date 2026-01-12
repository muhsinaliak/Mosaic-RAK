/**
 * @file config_manager.h
 * @brief Configuration Manager - LittleFS Based Storage
 * @version 1.0.0
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// Connection mode enum
enum ConnectionMode {
    CONN_MODE_NONE = 0,     // Not configured - start AP mode
    CONN_MODE_WIFI = 1,     // WiFi selected
    CONN_MODE_ETHERNET = 2  // Ethernet selected
};

// Konfigürasyon yapısı
struct GatewayConfig {
    // Network
    uint8_t     connection_mode;    // ConnectionMode enum
    char        wifi_ssid[64];
    char        wifi_password[64];
    bool        use_static_ip;
    char        static_ip[16];
    char        gateway[16];
    char        subnet[16];
    char        dns[16];

    // MQTT
    char        mqtt_server[128];
    uint16_t    mqtt_port;
    char        mqtt_user[64];
    char        mqtt_password[64];
    char        mqtt_client_id[64];

    // LoRa (for future use)
    uint32_t    lora_frequency;
    uint8_t     lora_sf;
    uint8_t     lora_bw;
    int8_t      lora_tx_power;

    // System
    char        device_name[64];
    uint8_t     led_brightness;
    bool        debug_enabled;
};

class ConfigManager {
public:
    /**
     * @brief Constructor
     */
    ConfigManager();

    /**
     * @brief Initialize file system and load config
     */
    bool begin();

    /**
     * @brief Load configuration from file
     */
    bool load();

    /**
     * @brief Save configuration to file
     */
    bool save();

    /**
     * @brief Reset to default values
     */
    void resetToDefaults();

    /**
     * @brief Format file system (factory reset)
     */
    bool format();

    /**
     * @brief Get config reference
     */
    GatewayConfig& getConfig() { return _config; }

    /**
     * @brief Get const config reference
     */
    const GatewayConfig& getConfig() const { return _config; }

    /**
     * @brief Check if file system is mounted
     */
    bool isMounted() const { return _mounted; }

    /**
     * @brief Get free space in bytes
     */
    size_t getFreeSpace() const;

    /**
     * @brief Get total space in bytes
     */
    size_t getTotalSpace() const;

    /**
     * @brief Get used space in bytes
     */
    size_t getUsedSpace() const;

    // Convenience setters
    void setConnectionMode(ConnectionMode mode);
    void setWiFi(const String& ssid, const String& password);
    void setMQTT(const String& server, uint16_t port, const String& user = "", const String& password = "");
    void setDeviceName(const String& name);
    void setLEDBrightness(uint8_t brightness);

    // Convenience getters
    ConnectionMode getConnectionMode() const;
    String getWiFiSSID() const;
    String getWiFiPassword() const;
    String getMQTTServer() const;
    uint16_t getMQTTPort() const;
    String getMQTTUser() const;
    String getMQTTPassword() const;
    String getDeviceName() const;

    /**
     * @brief Export config as JSON
     */
    String toJSON() const;

    /**
     * @brief Import config from JSON
     */
    bool fromJSON(const String& json);

private:
    GatewayConfig   _config;
    bool            _mounted;
    bool            _dirty;

    void            _setDefaults();
    bool            _initFS();
};

// Global instance
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
