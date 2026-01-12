/**
 * @file config_manager.cpp
 * @brief Configuration Manager Implementation
 * @version 1.0.0
 */

#include "config_manager.h"
#include <LittleFS.h>

// Global instance
ConfigManager configManager;

ConfigManager::ConfigManager()
    : _mounted(false)
    , _dirty(false)
{
    _setDefaults();
}

bool ConfigManager::begin() {
    LOG_INFO("CONFIG", "Initializing file system...");

    if (!_initFS()) {
        LOG_ERROR("CONFIG", "Failed to mount LittleFS!");
        return false;
    }

    _mounted = true;
    LOG_INFO("CONFIG", "LittleFS mounted successfully");

    // Load configuration
    if (!load()) {
        LOG_WARN("CONFIG", "No config file found, using defaults");
        save();  // Save defaults
    }

    return true;
}

bool ConfigManager::_initFS() {
    if (!LittleFS.begin(true)) {  // true = format if mount fails
        return false;
    }
    return true;
}

void ConfigManager::_setDefaults() {
    memset(&_config, 0, sizeof(GatewayConfig));

    // Network defaults
    _config.connection_mode = CONN_MODE_NONE;  // Not configured - will start AP mode
    strcpy(_config.wifi_ssid, "");
    strcpy(_config.wifi_password, "");
    _config.use_static_ip = false;
    strcpy(_config.static_ip, "0.0.0.0");
    strcpy(_config.gateway, "0.0.0.0");
    strcpy(_config.subnet, "255.255.255.0");
    strcpy(_config.dns, "8.8.8.8");

    // MQTT defaults
    strcpy(_config.mqtt_server, "");
    _config.mqtt_port = MQTT_DEFAULT_PORT;
    strcpy(_config.mqtt_user, "");
    strcpy(_config.mqtt_password, "");
    strcpy(_config.mqtt_client_id, "");

    // LoRa defaults
    _config.lora_frequency = 868000000;
    _config.lora_sf = 7;
    _config.lora_bw = 0;  // 125kHz
    _config.lora_tx_power = 14;

    // System defaults
    strcpy(_config.device_name, DEVICE_NAME);
    _config.led_brightness = 50;
    _config.debug_enabled = false;
}

bool ConfigManager::load() {
    if (!_mounted) return false;

    File file = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!file) {
        return false;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_ERROR("CONFIG", "JSON parse error");
        return false;
    }

    // Connection mode
    _config.connection_mode = doc["connection_mode"] | CONN_MODE_NONE;

    // Network
    strlcpy(_config.wifi_ssid, doc["wifi_ssid"] | "", sizeof(_config.wifi_ssid));
    strlcpy(_config.wifi_password, doc["wifi_password"] | "", sizeof(_config.wifi_password));
    _config.use_static_ip = doc["use_static_ip"] | false;
    strlcpy(_config.static_ip, doc["static_ip"] | "0.0.0.0", sizeof(_config.static_ip));
    strlcpy(_config.gateway, doc["gateway"] | "0.0.0.0", sizeof(_config.gateway));
    strlcpy(_config.subnet, doc["subnet"] | "255.255.255.0", sizeof(_config.subnet));
    strlcpy(_config.dns, doc["dns"] | "8.8.8.8", sizeof(_config.dns));

    // MQTT
    strlcpy(_config.mqtt_server, doc["mqtt_server"] | "", sizeof(_config.mqtt_server));
    _config.mqtt_port = doc["mqtt_port"] | MQTT_DEFAULT_PORT;
    strlcpy(_config.mqtt_user, doc["mqtt_user"] | "", sizeof(_config.mqtt_user));
    strlcpy(_config.mqtt_password, doc["mqtt_password"] | "", sizeof(_config.mqtt_password));
    strlcpy(_config.mqtt_client_id, doc["mqtt_client_id"] | "", sizeof(_config.mqtt_client_id));

    // LoRa
    _config.lora_frequency = doc["lora_frequency"] | 868000000;
    _config.lora_sf = doc["lora_sf"] | 7;
    _config.lora_bw = doc["lora_bw"] | 0;
    _config.lora_tx_power = doc["lora_tx_power"] | 14;

    // System
    strlcpy(_config.device_name, doc["device_name"] | DEVICE_NAME, sizeof(_config.device_name));
    _config.led_brightness = doc["led_brightness"] | 50;
    _config.debug_enabled = doc["debug_enabled"] | false;

    LOG_INFO("CONFIG", "Configuration loaded");
    return true;
}

bool ConfigManager::save() {
    if (!_mounted) return false;

    JsonDocument doc;

    // Connection mode
    doc["connection_mode"] = _config.connection_mode;

    // Network
    doc["wifi_ssid"] = _config.wifi_ssid;
    doc["wifi_password"] = _config.wifi_password;
    doc["use_static_ip"] = _config.use_static_ip;
    doc["static_ip"] = _config.static_ip;
    doc["gateway"] = _config.gateway;
    doc["subnet"] = _config.subnet;
    doc["dns"] = _config.dns;

    // MQTT
    doc["mqtt_server"] = _config.mqtt_server;
    doc["mqtt_port"] = _config.mqtt_port;
    doc["mqtt_user"] = _config.mqtt_user;
    doc["mqtt_password"] = _config.mqtt_password;
    doc["mqtt_client_id"] = _config.mqtt_client_id;

    // LoRa
    doc["lora_frequency"] = _config.lora_frequency;
    doc["lora_sf"] = _config.lora_sf;
    doc["lora_bw"] = _config.lora_bw;
    doc["lora_tx_power"] = _config.lora_tx_power;

    // System
    doc["device_name"] = _config.device_name;
    doc["led_brightness"] = _config.led_brightness;
    doc["debug_enabled"] = _config.debug_enabled;

    File file = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!file) {
        LOG_ERROR("CONFIG", "Failed to open config file for writing");
        return false;
    }

    serializeJsonPretty(doc, file);
    file.close();

    _dirty = false;
    LOG_INFO("CONFIG", "Configuration saved");
    return true;
}

void ConfigManager::resetToDefaults() {
    _setDefaults();
    _dirty = true;
}

bool ConfigManager::format() {
    if (!_mounted) return false;

    LOG_WARN("CONFIG", "Resetting configuration to factory defaults...");

    // Only delete the config file, NOT format entire filesystem
    // This preserves SPA files (index.html, script.js, style.css)
    if (LittleFS.exists(CONFIG_FILE_PATH)) {
        LittleFS.remove(CONFIG_FILE_PATH);
        LOG_INFO("CONFIG", "Config file deleted");
    }

    // Reset to defaults
    _setDefaults();

    // Save default config
    save();

    LOG_INFO("CONFIG", "Factory reset complete");
    return true;
}

size_t ConfigManager::getFreeSpace() const {
    if (!_mounted) return 0;
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t ConfigManager::getTotalSpace() const {
    if (!_mounted) return 0;
    return LittleFS.totalBytes();
}

size_t ConfigManager::getUsedSpace() const {
    if (!_mounted) return 0;
    return LittleFS.usedBytes();
}

void ConfigManager::setWiFi(const String& ssid, const String& password) {
    strlcpy(_config.wifi_ssid, ssid.c_str(), sizeof(_config.wifi_ssid));
    strlcpy(_config.wifi_password, password.c_str(), sizeof(_config.wifi_password));
    _dirty = true;
}

void ConfigManager::setMQTT(const String& server, uint16_t port, const String& user, const String& password) {
    strlcpy(_config.mqtt_server, server.c_str(), sizeof(_config.mqtt_server));
    _config.mqtt_port = port;
    strlcpy(_config.mqtt_user, user.c_str(), sizeof(_config.mqtt_user));
    strlcpy(_config.mqtt_password, password.c_str(), sizeof(_config.mqtt_password));
    _dirty = true;
}

void ConfigManager::setDeviceName(const String& name) {
    strlcpy(_config.device_name, name.c_str(), sizeof(_config.device_name));
    _dirty = true;
}

void ConfigManager::setLEDBrightness(uint8_t brightness) {
    _config.led_brightness = brightness;
    _dirty = true;
}

void ConfigManager::setConnectionMode(ConnectionMode mode) {
    _config.connection_mode = mode;
    _dirty = true;
}

ConnectionMode ConfigManager::getConnectionMode() const {
    return static_cast<ConnectionMode>(_config.connection_mode);
}

String ConfigManager::getWiFiSSID() const {
    return String(_config.wifi_ssid);
}

String ConfigManager::getWiFiPassword() const {
    return String(_config.wifi_password);
}

String ConfigManager::getMQTTServer() const {
    return String(_config.mqtt_server);
}

uint16_t ConfigManager::getMQTTPort() const {
    return _config.mqtt_port;
}

String ConfigManager::getMQTTUser() const {
    return String(_config.mqtt_user);
}

String ConfigManager::getMQTTPassword() const {
    return String(_config.mqtt_password);
}

String ConfigManager::getDeviceName() const {
    return String(_config.device_name);
}

String ConfigManager::toJSON() const {
    JsonDocument doc;

    doc["connection_mode"] = _config.connection_mode;
    doc["wifi_ssid"] = _config.wifi_ssid;
    doc["use_static_ip"] = _config.use_static_ip;
    doc["mqtt_server"] = _config.mqtt_server;
    doc["mqtt_port"] = _config.mqtt_port;
    doc["device_name"] = _config.device_name;
    doc["led_brightness"] = _config.led_brightness;
    doc["lora_frequency"] = _config.lora_frequency;
    doc["lora_sf"] = _config.lora_sf;

    String output;
    serializeJson(doc, output);
    return output;
}

bool ConfigManager::fromJSON(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        return false;
    }

    if (doc.containsKey("connection_mode")) {
        _config.connection_mode = doc["connection_mode"];
    }
    if (doc.containsKey("wifi_ssid")) {
        strlcpy(_config.wifi_ssid, doc["wifi_ssid"], sizeof(_config.wifi_ssid));
    }
    if (doc.containsKey("wifi_password")) {
        strlcpy(_config.wifi_password, doc["wifi_password"], sizeof(_config.wifi_password));
    }
    if (doc.containsKey("mqtt_server")) {
        strlcpy(_config.mqtt_server, doc["mqtt_server"], sizeof(_config.mqtt_server));
    }
    if (doc.containsKey("mqtt_port")) {
        _config.mqtt_port = doc["mqtt_port"];
    }
    if (doc.containsKey("mqtt_user")) {
        strlcpy(_config.mqtt_user, doc["mqtt_user"], sizeof(_config.mqtt_user));
    }
    if (doc.containsKey("mqtt_password")) {
        strlcpy(_config.mqtt_password, doc["mqtt_password"], sizeof(_config.mqtt_password));
    }
    if (doc.containsKey("device_name")) {
        strlcpy(_config.device_name, doc["device_name"], sizeof(_config.device_name));
    }
    if (doc.containsKey("led_brightness")) {
        _config.led_brightness = doc["led_brightness"];
    }
    if (doc.containsKey("use_static_ip")) {
        _config.use_static_ip = doc["use_static_ip"];
    }
    if (doc.containsKey("static_ip")) {
        strlcpy(_config.static_ip, doc["static_ip"], sizeof(_config.static_ip));
    }
    if (doc.containsKey("gateway")) {
        strlcpy(_config.gateway, doc["gateway"], sizeof(_config.gateway));
    }
    if (doc.containsKey("subnet")) {
        strlcpy(_config.subnet, doc["subnet"], sizeof(_config.subnet));
    }
    if (doc.containsKey("dns")) {
        strlcpy(_config.dns, doc["dns"], sizeof(_config.dns));
    }

    _dirty = true;
    return true;
}
