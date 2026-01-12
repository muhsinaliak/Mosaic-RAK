/**
 * @file web_server.cpp
 * @brief Web Server & REST API Implementation
 * @version 1.0.0
 */

#include "web_server.h"
#include "config_manager.h"
#include "network_manager.h"
#include "mqtt_client.h"
#include "lora_manager.h"
#include "led_controller.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>
#include <HTTPClient.h>

// Global instance
WebServerManager webServerManager;

// OTA Update state
static struct {
    bool inProgress = false;
    String status = "idle";      // idle, downloading, installing, complete, error
    int progress = 0;
    String error = "";
    size_t totalSize = 0;
    size_t currentSize = 0;
} otaState;

// ============================================================================
// CONSTRUCTOR & INITIALIZATION
// ============================================================================

WebServerManager::WebServerManager()
    : _server(WEB_SERVER_PORT)
    , _running(false)
{
}

bool WebServerManager::begin(uint16_t port) {
    LOG_INFO("WEB", "Starting Web Server...");

    _setupRoutes();
    _server.begin(port);
    _running = true;

    Serial.printf("[WEB] Server started on port %d\n", port);
    return true;
}

void WebServerManager::update() {
    if (_running) {
        _server.handleClient();
    }
}

void WebServerManager::stop() {
    _server.stop();
    _running = false;
    LOG_INFO("WEB", "Server stopped");
}

// ============================================================================
// ROUTE SETUP
// ============================================================================

void WebServerManager::_setupRoutes() {
    _setupAPIRoutes();
    _setupStaticRoutes();
}

void WebServerManager::_setupAPIRoutes() {
    // CORS preflight
    _server.on("/api/status", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/scan", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/scan-results", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/add", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/nodes", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/control", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/config", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/reboot", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/wifi-scan", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/mqtt-publish", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/wifi-connect", HTTP_OPTIONS, [this]() { _handleOptions(); });
    _server.on("/api/ethernet-status", HTTP_OPTIONS, [this]() { _handleOptions(); });

    // GET endpoints
    _server.on("/api/status", HTTP_GET, [this]() { _handleStatus(); });
    _server.on("/api/scan", HTTP_GET, [this]() { _handleScan(); });
    _server.on("/api/scan-results", HTTP_GET, [this]() { _handleScanResults(); });
    _server.on("/api/nodes", HTTP_GET, [this]() { _handleNodes(); });
    _server.on("/api/config", HTTP_GET, [this]() { _handleConfig(); });
    _server.on("/api/wifi-scan", HTTP_GET, [this]() { _handleWifiScan(); });
    _server.on("/api/ethernet-status", HTTP_GET, [this]() { _handleEthernetStatus(); });

    // POST endpoints
    _server.on("/api/add", HTTP_POST, [this]() { _handleAddNode(); });
    _server.on("/api/control", HTTP_POST, [this]() { _handleNodeControl(); });
    _server.on("/api/config", HTTP_POST, [this]() { _handleSaveConfig(); });
    _server.on("/api/reboot", HTTP_POST, [this]() { _handleReboot(); });
    _server.on("/api/factory-reset", HTTP_POST, [this]() { _handleFactoryReset(); });
    _server.on("/api/mqtt-publish", HTTP_POST, [this]() { _handleMQTTPublish(); });
    _server.on("/api/mqtt-connect", HTTP_POST, [this]() { _handleMQTTConnect(); });
    _server.on("/api/wifi-connect", HTTP_POST, [this]() { _handleWifiConnect(); });
    _server.on("/api/ethernet-connect", HTTP_POST, [this]() { _handleEthernetConnect(); });

    // DELETE endpoints
    _server.on("/api/nodes", HTTP_DELETE, [this]() { _handleRemoveNode(); });

    // OTA Update endpoints
    _server.on("/api/update", HTTP_POST, [this]() { _handleFirmwareUpdate(); },
        [this]() { _handleFirmwareUpload(); });
    _server.on("/api/update-fs", HTTP_POST, [this]() { _handleFilesystemUpdate(); },
        [this]() { _handleFilesystemUpload(); });
    _server.on("/api/github-release", HTTP_POST, [this]() { _handleGithubRelease(); });
    _server.on("/api/github-update", HTTP_POST, [this]() { _handleGithubUpdate(); });
    _server.on("/api/update-progress", HTTP_GET, [this]() { _handleUpdateProgress(); });
}

void WebServerManager::_setupStaticRoutes() {
    // Serve static files from LittleFS
    _server.on("/", HTTP_GET, [this]() { _handleRoot(); });
    _server.on("/style.css", HTTP_GET, [this]() { _serveStaticFile("/style.css", "text/css"); });
    _server.on("/script.js", HTTP_GET, [this]() { _serveStaticFile("/script.js", "application/javascript"); });
    _server.onNotFound([this]() { _handleNotFound(); });
}

// ============================================================================
// CORS HANDLER
// ============================================================================

void WebServerManager::_handleOptions() {
    CORS_HEADERS();
    _server.send(204);
}

// ============================================================================
// API: GET /api/status
// ============================================================================

void WebServerManager::_handleStatus() {
    CORS_HEADERS();

    JsonDocument doc;

    // System info
    doc["version"] = GATEWAY_VERSION;
    doc["build_date"] = __DATE__ " " __TIME__;
    doc["uptime"] = millis() / 1000;
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_total"] = ESP.getHeapSize();

    // Network status - prioritize Ethernet over AP mode
    bool ethConnected = networkManager.isEthernetConnected();
    bool wifiConnected = networkManager.isConnected() && networkManager.getConnectionType() == "WiFi";

    if (ethConnected) {
        doc["network"]["connected"] = true;
        doc["network"]["type"] = "Ethernet";
        doc["network"]["ip"] = networkManager.getEthernetIP().toString();
        doc["network"]["rssi"] = 0;
    } else if (wifiConnected) {
        doc["network"]["connected"] = true;
        doc["network"]["type"] = "WiFi";
        doc["network"]["ip"] = networkManager.getIP().toString();
        doc["network"]["rssi"] = networkManager.getRSSI();
    } else {
        doc["network"]["connected"] = false;
        doc["network"]["type"] = networkManager.getConnectionType();
        doc["network"]["ip"] = networkManager.getIP().toString();
        doc["network"]["rssi"] = networkManager.getRSSI();
    }

    // MQTT status
    doc["mqtt"]["connected"] = mqttClient.isConnected();
    doc["mqtt"]["server"] = configManager.getMQTTServer();

    // LoRa status
    doc["lora"]["scanning"] = loraManager.isScanning();
    doc["lora"]["nodes_registered"] = loraManager.getRegisteredNodeCount();
    doc["lora"]["nodes_online"] = loraManager.getOnlineNodeCount();

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: GET /api/scan
// ============================================================================

void WebServerManager::_handleScan() {
    CORS_HEADERS();

    // Parse optional duration parameter
    uint32_t duration = LORA_SCAN_DURATION;
    if (_server.hasArg("duration")) {
        duration = _server.arg("duration").toInt();
        if (duration < 5000) duration = 5000;
        if (duration > 120000) duration = 120000;
    }

    // Start scan
    loraManager.startScan(duration);

    // Visual feedback
    statusLED.setStatus(SYS_STATUS_AP_MODE);  // Blue breathing

    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Scan started";
    doc["duration"] = duration;

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: GET /api/scan-results
// ============================================================================

void WebServerManager::_handleScanResults() {
    CORS_HEADERS();

    JsonDocument doc;
    doc["scanning"] = loraManager.isScanning();

    JsonArray devices = doc["devices"].to<JsonArray>();

    const auto& discovered = loraManager.getDiscoveredNodes();
    for (const auto& node : discovered) {
        if (!node.valid) continue;

        JsonObject dev = devices.add<JsonObject>();
        dev["mac"] = LoRaManager::macToString(node.macAddr);
        dev["type"] = node.deviceType;
        dev["type_name"] = (node.deviceType == DEV_TYPE_RELAY_2CH) ? "Relay 2CH" :
                           (node.deviceType == DEV_TYPE_RELAY_4CH) ? "Relay 4CH" :
                           (node.deviceType == DEV_TYPE_SENSOR) ? "Sensor" : "Unknown";
        dev["fw_version"] = node.fwVersion;
        dev["rssi"] = node.rssi;
        dev["snr"] = node.snr;
        dev["discovered_ago"] = (millis() - node.discoveredAt) / 1000;
    }

    doc["count"] = devices.size();

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: POST /api/add
// ============================================================================

void WebServerManager::_handleAddNode() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("mac")) {
        _sendError(400, "Missing 'mac' field");
        return;
    }

    String macStr = doc["mac"].as<String>();
    uint8_t mac[MAC_ADDR_LEN];

    if (!LoRaManager::stringToMAC(macStr, mac)) {
        _sendError(400, "Invalid MAC address format");
        return;
    }

    // Start pairing
    if (loraManager.startPairing(mac)) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = "Pairing started";
        resp["mac"] = macStr;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    } else {
        _sendError(400, "Failed to start pairing. Node may already be registered.");
    }
}

// ============================================================================
// API: GET /api/nodes
// ============================================================================

void WebServerManager::_handleNodes() {
    CORS_HEADERS();

    JsonDocument doc;
    JsonArray nodes = doc["nodes"].to<JsonArray>();

    const auto& registered = loraManager.getRegisteredNodes();
    for (const auto& node : registered) {
        if (!node.valid) continue;

        JsonObject n = nodes.add<JsonObject>();
        n["id"] = node.nodeID;
        n["mac"] = LoRaManager::macToString(node.macAddr);
        n["name"] = node.name;
        n["type"] = node.deviceType;
        n["type_name"] = (node.deviceType == DEV_TYPE_RELAY_2CH) ? "Relay 2CH" :
                         (node.deviceType == DEV_TYPE_RELAY_4CH) ? "Relay 4CH" :
                         (node.deviceType == DEV_TYPE_SENSOR) ? "Sensor" : "Unknown";
        n["online"] = node.online;
        n["relay_status"] = node.relayStatus;

        // Relay states as array
        JsonArray relays = n["relays"].to<JsonArray>();
        relays.add((node.relayStatus & RELAY_1_BIT) != 0);
        relays.add((node.relayStatus & RELAY_2_BIT) != 0);

        n["rssi"] = node.lastRSSI;
        n["snr"] = node.lastSNR;
        n["uptime"] = node.uptime;
        n["last_seen"] = node.online ? (millis() - node.lastSeen) / 1000 : -1;
    }

    doc["count"] = nodes.size();
    doc["online"] = loraManager.getOnlineNodeCount();

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: POST /api/control
// ============================================================================

void WebServerManager::_handleNodeControl() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("node_id")) {
        _sendError(400, "Missing 'node_id' field");
        return;
    }

    uint8_t nodeID = doc["node_id"];

    // Check if node exists
    RegisteredNode* node = loraManager.getNodeByID(nodeID);
    if (!node) {
        _sendError(404, "Node not found");
        return;
    }

    bool success = false;
    String action = "";

    if (doc.containsKey("relay_bitmap")) {
        // Set all relays
        uint8_t bitmap = doc["relay_bitmap"];
        success = loraManager.sendRelayCommand(nodeID, bitmap);
        action = "set_relays";
    } else if (doc.containsKey("toggle_relay")) {
        // Toggle specific relay
        uint8_t relayNum = doc["toggle_relay"];
        success = loraManager.sendRelayToggle(nodeID, relayNum);
        action = "toggle_relay";
    } else if (doc.containsKey("action")) {
        String actionStr = doc["action"].as<String>();
        if (actionStr == "status") {
            success = loraManager.requestNodeStatus(nodeID);
            action = "request_status";
        } else if (actionStr == "reset") {
            success = loraManager.sendResetCommand(nodeID);
            action = "reset";
        }
    } else {
        _sendError(400, "No valid command specified");
        return;
    }

    if (success) {
        JsonDocument resp;
        resp["success"] = true;
        resp["action"] = action;
        resp["node_id"] = nodeID;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    } else {
        _sendError(500, "Failed to send command");
    }
}

// ============================================================================
// API: DELETE /api/nodes?id=X
// ============================================================================

void WebServerManager::_handleRemoveNode() {
    CORS_HEADERS();

    if (!_server.hasArg("id")) {
        _sendError(400, "Missing 'id' parameter");
        return;
    }

    uint8_t nodeID = _server.arg("id").toInt();

    if (loraManager.removeNode(nodeID)) {
        _sendSuccess("Node removed");
    } else {
        _sendError(404, "Node not found");
    }
}

// ============================================================================
// API: GET /api/config
// ============================================================================

void WebServerManager::_handleConfig() {
    CORS_HEADERS();

    JsonDocument doc;

    doc["connection_mode"] = configManager.getConnectionMode();
    doc["device_name"] = configManager.getDeviceName();
    doc["wifi_ssid"] = configManager.getWiFiSSID();
    doc["mqtt_server"] = configManager.getMQTTServer();
    doc["mqtt_port"] = configManager.getMQTTPort();
    doc["mqtt_user"] = configManager.getMQTTUser();
    doc["led_brightness"] = configManager.getConfig().led_brightness;

    // Network Config
    doc["use_static_ip"] = configManager.getConfig().use_static_ip;
    doc["static_ip"] = configManager.getConfig().static_ip;
    doc["gateway"] = configManager.getConfig().gateway;
    doc["subnet"] = configManager.getConfig().subnet;
    doc["dns"] = configManager.getConfig().dns;

    // LoRa config
    doc["lora"]["frequency"] = LORA_FREQUENCY;
    doc["lora"]["sf"] = LORA_SF;
    doc["lora"]["bw"] = LORA_BW;
    doc["lora"]["tx_power"] = LORA_TX_POWER;

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: POST /api/config
// ============================================================================

void WebServerManager::_handleSaveConfig() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    bool needRestart = false;

    // Update WiFi
    if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password")) {
        configManager.setWiFi(
            doc["wifi_ssid"].as<String>(),
            doc["wifi_password"].as<String>()
        );
        needRestart = true;
    }

    // Update MQTT
    if (doc.containsKey("mqtt_server")) {
        configManager.setMQTT(
            doc["mqtt_server"].as<String>(),
            doc["mqtt_port"] | MQTT_DEFAULT_PORT,
            doc["mqtt_user"] | "",
            doc["mqtt_password"] | ""
        );
    }

    // Update device name
    if (doc.containsKey("device_name")) {
        configManager.setDeviceName(doc["device_name"].as<String>());
    }

    // Update LED brightness
    if (doc.containsKey("led_brightness")) {
        uint8_t brightness = doc["led_brightness"];
        configManager.setLEDBrightness(brightness);
        statusLED.setBrightness(brightness);  // Apply immediately
    }

    // Update Network IP Settings
    if (doc.containsKey("use_static_ip")) {
        GatewayConfig& config = configManager.getConfig();
        config.use_static_ip = doc["use_static_ip"];
        if (doc.containsKey("static_ip")) strlcpy(config.static_ip, doc["static_ip"], sizeof(config.static_ip));
        if (doc.containsKey("gateway")) strlcpy(config.gateway, doc["gateway"], sizeof(config.gateway));
        if (doc.containsKey("subnet")) strlcpy(config.subnet, doc["subnet"], sizeof(config.subnet));
        if (doc.containsKey("dns")) strlcpy(config.dns, doc["dns"], sizeof(config.dns));

        needRestart = true;
    }

    // Update Connection Mode (WiFi = 1, Ethernet = 2)
    if (doc.containsKey("connection_mode")) {
        uint8_t mode = doc["connection_mode"];
        configManager.setConnectionMode(static_cast<ConnectionMode>(mode));
        needRestart = true;
    }

    // Save
    if (configManager.save()) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = needRestart ? "Config saved. Restart required for network changes." : "Config saved";
        resp["restart_required"] = needRestart;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    } else {
        _sendError(500, "Failed to save config");
    }
}

// ============================================================================
// API: POST /api/reboot
// ============================================================================

void WebServerManager::_handleReboot() {
    CORS_HEADERS();

    _sendSuccess("Rebooting...");

    // Delay to send response
    uint32_t start = millis();
    while (millis() - start < 500) {
        _server.handleClient();
    }

    ESP.restart();
}

// ============================================================================
// API: POST /api/factory-reset
// ============================================================================

void WebServerManager::_handleFactoryReset() {
    CORS_HEADERS();

    _sendSuccess("Factory reset initiated...");

    // Delay to send response
    uint32_t start = millis();
    while (millis() - start < 500) {
        _server.handleClient();
    }

    configManager.format();
    ESP.restart();
}

// ============================================================================
// API: GET /api/wifi-scan
// ============================================================================

void WebServerManager::_handleWifiScan() {
    CORS_HEADERS();

    LOG_INFO("WEB", "Starting WiFi scan...");

    // Visual feedback - blue blinking during scan (3 blinks)
    statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);

    // Start WiFi scan
    int numNetworks = WiFi.scanNetworks(false, true);  // async=false, show_hidden=true

    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();

    if (numNetworks > 0) {
        for (int i = 0; i < numNetworks && i < 20; i++) {  // Max 20 networks
            JsonObject net = networks.add<JsonObject>();
            net["ssid"] = WiFi.SSID(i);
            net["rssi"] = WiFi.RSSI(i);
            net["channel"] = WiFi.channel(i);
            net["encryption"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
    }

    doc["count"] = numNetworks > 0 ? numNetworks : 0;

    // Clean up scan results
    WiFi.scanDelete();

    // Restore LED status based on connection state
    if (networkManager.isConnected()) {
        statusLED.setStatus(SYS_STATUS_ONLINE);
    } else {
        statusLED.setStatus(SYS_STATUS_AP_MODE);
    }

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// API: POST /api/mqtt-publish
// ============================================================================

void WebServerManager::_handleMQTTPublish() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    if (!doc.containsKey("topic") || !doc.containsKey("message")) {
        _sendError(400, "Missing 'topic' or 'message' field");
        return;
    }

    if (!mqttClient.isConnected()) {
        _sendError(503, "MQTT not connected");
        return;
    }

    String topic = doc["topic"].as<String>();
    String message = doc["message"].as<String>();
    bool retained = doc["retained"] | false;

    if (mqttClient.publish(topic, message, retained)) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = "Message published";
        resp["topic"] = topic;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    } else {
        _sendError(500, "Failed to publish message");
    }
}

// ============================================================================
// API: POST /api/mqtt-connect
// ============================================================================

void WebServerManager::_handleMQTTConnect() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    String server = doc["mqtt_server"] | "";
    uint16_t port = doc["mqtt_port"] | 1883;
    String user = doc["mqtt_user"] | "";
    String password = doc["mqtt_password"] | "";

    if (server.length() == 0) {
        _sendError(400, "Missing mqtt_server");
        return;
    }

    // Check if network is connected
    if (!networkManager.isConnected()) {
        _sendError(503, "Network not connected");
        return;
    }

    // Save MQTT settings to config
    configManager.setMQTT(server, port, user, password);
    configManager.save();

    LOG_INFO("WEB", ("Testing MQTT connection to: " + server).c_str());

    // Disconnect existing connection if any
    if (mqttClient.isConnected()) {
        mqttClient.disconnect();
    }

    // Update MQTT client settings
    mqttClient.setServer(server, port);
    mqttClient.setAuth(user, password);

    // Re-initialize and try to connect
    mqttClient.begin(server, port, "");

    // Attempt connection
    bool connected = mqttClient.connect();

    if (connected) {
        JsonDocument resp;
        resp["success"] = true;
        resp["message"] = "MQTT connected successfully";
        resp["server"] = server;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    } else {
        // Get detailed error from PubSubClient
        int state = mqttClient.getState();
        String errorMsg;

        switch (state) {
            case -4: errorMsg = "Connection timeout - server unreachable"; break;
            case -3: errorMsg = "Connection lost"; break;
            case -2: errorMsg = "Connection failed - check server address"; break;
            case -1: errorMsg = "Disconnected"; break;
            case 1:  errorMsg = "Bad protocol version"; break;
            case 2:  errorMsg = "Bad client ID"; break;
            case 3:  errorMsg = "Server unavailable"; break;
            case 4:  errorMsg = "Bad credentials - check username/password"; break;
            case 5:  errorMsg = "Unauthorized - authentication required"; break;
            default: errorMsg = "Connection failed - check server address, username and password"; break;
        }

        LOG_ERROR("MQTT", ("Connection error (state=" + String(state) + "): " + errorMsg).c_str());

        JsonDocument resp;
        resp["success"] = false;
        resp["error"] = errorMsg;
        resp["state"] = state;

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);
    }
}

// ============================================================================
// API: POST /api/wifi-connect
// ============================================================================

void WebServerManager::_handleWifiConnect() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error || !doc.containsKey("ssid")) {
        _sendError(400, "Invalid request - missing ssid");
        return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"] | "";

    // Save credentials and set connection mode to WiFi
    configManager.setWiFi(ssid, password);
    configManager.setConnectionMode(CONN_MODE_WIFI);

    // Handle static IP settings if provided
    if (doc.containsKey("use_static_ip")) {
        GatewayConfig& config = configManager.getConfig();
        config.use_static_ip = doc["use_static_ip"];
        if (doc.containsKey("static_ip")) strlcpy(config.static_ip, doc["static_ip"], sizeof(config.static_ip));
        if (doc.containsKey("gateway")) strlcpy(config.gateway, doc["gateway"], sizeof(config.gateway));
        if (doc.containsKey("subnet")) strlcpy(config.subnet, doc["subnet"], sizeof(config.subnet));
        if (doc.containsKey("dns")) strlcpy(config.dns, doc["dns"], sizeof(config.dns));
    }

    // Save MQTT if provided
    if (doc.containsKey("mqtt_server")) {
        configManager.setMQTT(
            doc["mqtt_server"].as<String>(),
            doc["mqtt_port"] | MQTT_DEFAULT_PORT,
            doc["mqtt_user"] | "",
            doc["mqtt_password"] | ""
        );
    }

    configManager.save();

    // Visual feedback - blue blinking during connection attempt
    statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);
    LOG_INFO("WEB", ("Trying to connect to WiFi: " + ssid).c_str());

    // Disconnect current WiFi
    WiFi.disconnect(false);
    delay(200);

    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    delay(100);

    // Apply static IP if configured
    const GatewayConfig& config = configManager.getConfig();
    if (config.use_static_ip && strlen(config.static_ip) > 0) {
        IPAddress ip, gateway, subnet, dns;
        ip.fromString(config.static_ip);
        gateway.fromString(config.gateway);
        subnet.fromString(config.subnet);
        dns.fromString(config.dns);
        LOG_INFO("WEB", ("WiFi static IP: " + String(config.static_ip)).c_str());
        WiFi.config(ip, gateway, subnet, dns);
    }

    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection (max 20 seconds with LED feedback)
    uint32_t startTime = millis();
    uint32_t lastBlink = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
        delay(100);
        // Blink blue every 500ms during connection attempt
        if (millis() - lastBlink >= 500) {
            statusLED.blink(LED_COLOR_BLUE, 1, 100, 100);
            lastBlink = millis();
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        String newIP = WiFi.localIP().toString();
        LOG_INFO("WEB", ("Connected! IP: " + newIP).c_str());

        JsonDocument resp;
        resp["success"] = true;
        resp["ip"] = newIP;
        resp["message"] = "Connected successfully";

        String json;
        serializeJson(resp, json);
        _sendJSON(200, json);

        // Wait for response to be sent, then restart to apply all settings
        delay(1000);
        ESP.restart();
    } else {
        // Connection failed
        wl_status_t status = WiFi.status();
        statusLED.setStatus(SYS_STATUS_OFFLINE);

        String errorMsg = "Connection failed";
        if (status == WL_NO_SSID_AVAIL) {
            errorMsg = "Network not found";
        } else if (status == WL_CONNECT_FAILED) {
            errorMsg = "Authentication failed - check password";
        } else if (status == WL_DISCONNECTED) {
            errorMsg = "Connection rejected - verify password";
        } else if (status == WL_IDLE_STATUS) {
            errorMsg = "WiFi module idle - try again";
        }

        LOG_WARN("WEB", ("WiFi connect failed. Status: " + String(status) + " - " + errorMsg).c_str());
        _sendError(400, errorMsg);
    }
}

// ============================================================================
// API: POST /api/ethernet-connect
// ============================================================================

void WebServerManager::_handleEthernetConnect() {
    CORS_HEADERS();

    String body = _getRequestBody();
    JsonDocument doc;

    if (body.length() > 0) {
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            _sendError(400, "Invalid JSON");
            return;
        }
    }

    // Save connection mode to Ethernet
    configManager.setConnectionMode(CONN_MODE_ETHERNET);

    // Handle static IP if provided
    if (doc.containsKey("use_static_ip")) {
        GatewayConfig& config = configManager.getConfig();
        config.use_static_ip = doc["use_static_ip"];
        if (doc.containsKey("static_ip")) strlcpy(config.static_ip, doc["static_ip"], sizeof(config.static_ip));
        if (doc.containsKey("gateway")) strlcpy(config.gateway, doc["gateway"], sizeof(config.gateway));
        if (doc.containsKey("subnet")) strlcpy(config.subnet, doc["subnet"], sizeof(config.subnet));
        if (doc.containsKey("dns")) strlcpy(config.dns, doc["dns"], sizeof(config.dns));
    }

    configManager.save();

    // Visual feedback
    statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);
    LOG_INFO("WEB", "Switching to Ethernet connection...");

    // Initialize Ethernet if not already done
    if (!networkManager.isEthernetInitialized()) {
        if (!networkManager.initEthernet()) {
            _sendError(500, "Failed to initialize Ethernet hardware");
            return;
        }
    }

    // Check if cable is connected
    if (!networkManager.isEthernetCableConnected()) {
        _sendError(400, "Ethernet cable not connected");
        return;
    }

    // Send success response first (we'll restart after)
    JsonDocument resp;
    resp["success"] = true;
    resp["message"] = "Ethernet configured, restarting...";

    String json;
    serializeJson(resp, json);
    _sendJSON(200, json);

    // Wait for response to be sent, then restart
    delay(1000);
    ESP.restart();
}

// ============================================================================
// API: GET /api/ethernet-status
// ============================================================================

void WebServerManager::_handleEthernetStatus() {
    CORS_HEADERS();

    JsonDocument doc;

    bool ethConnected = networkManager.isEthernetConnected();
    bool cableConnected = networkManager.isEthernetCableConnected();

    doc["cable_connected"] = cableConnected;
    doc["connected"] = ethConnected;

    if (ethConnected) {
        doc["ip"] = networkManager.getEthernetIP().toString();
        doc["success"] = true;
        doc["message"] = "Ethernet connected";
    } else if (cableConnected) {
        doc["ip"] = "";
        doc["success"] = false;
        doc["message"] = "Cable connected, waiting for DHCP";
    } else {
        doc["ip"] = "";
        doc["success"] = false;
        doc["message"] = "Ethernet cable not connected";
    }

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

// ============================================================================
// STATIC FILE HANDLERS
// ============================================================================

void WebServerManager::_handleRoot() {
    _serveStaticFile("/index.html", "text/html");
}

void WebServerManager::_serveStaticFile(const String& path, const String& contentType) {
    CORS_HEADERS();

    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        if (file) {
            _server.streamFile(file, contentType);
            file.close();
            return;
        }
    }

    // Fallback: File not found
    LOG_WARN("WEB", ("File not found: " + path).c_str());
    _sendError(404, "File not found: " + path);
}

void WebServerManager::_handleNotFound() {
    CORS_HEADERS();
    _sendError(404, "Not found");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void WebServerManager::_sendJSON(int code, const String& json) {
    _server.send(code, "application/json", json);
}

void WebServerManager::_sendError(int code, const String& message) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = message;

    String json;
    serializeJson(doc, json);
    _sendJSON(code, json);
}

void WebServerManager::_sendSuccess(const String& message) {
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = message;

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}

String WebServerManager::_getRequestBody() {
    if (_server.hasArg("plain")) {
        return _server.arg("plain");
    }
    return "";
}

// ============================================================================
// OTA UPDATE HANDLERS
// ============================================================================

void WebServerManager::_handleFirmwareUpload() {
    HTTPUpload& upload = _server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        LOG_INFO("OTA", ("Firmware update started: " + upload.filename).c_str());
        statusLED.setStatus(SYS_STATUS_OTA_UPDATE);
        otaState.inProgress = true;
        otaState.status = "installing";
        otaState.progress = 0;
        otaState.error = "";

        // Start update
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaState.status = "error";
            otaState.error = "Failed to start update";
            LOG_ERROR("OTA", "Update.begin() failed");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            otaState.status = "error";
            otaState.error = "Write failed";
            LOG_ERROR("OTA", "Update.write() failed");
        } else {
            // Update progress
            if (otaState.totalSize > 0) {
                otaState.currentSize += upload.currentSize;
                otaState.progress = (otaState.currentSize * 100) / otaState.totalSize;
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            otaState.status = "complete";
            otaState.progress = 100;
            LOG_INFO("OTA", ("Firmware update complete, size: " + String(upload.totalSize)).c_str());
        } else {
            otaState.status = "error";
            otaState.error = "Update failed: " + String(Update.errorString());
            LOG_ERROR("OTA", ("Update failed: " + String(Update.errorString())).c_str());
        }
        otaState.inProgress = false;
    }
}

void WebServerManager::_handleFirmwareUpdate() {
    CORS_HEADERS();

    if (otaState.status == "complete") {
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Firmware updated successfully. Rebooting...";

        String json;
        serializeJson(doc, json);
        _sendJSON(200, json);

        delay(1000);
        ESP.restart();
    } else if (otaState.status == "error") {
        _sendError(500, otaState.error);
        statusLED.setStatus(SYS_STATUS_ONLINE);
    } else {
        _sendError(400, "No update in progress");
    }
}

void WebServerManager::_handleFilesystemUpload() {
    HTTPUpload& upload = _server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        LOG_INFO("OTA", ("Filesystem update started: " + upload.filename).c_str());
        statusLED.setStatus(SYS_STATUS_OTA_UPDATE);
        otaState.inProgress = true;
        otaState.status = "installing";
        otaState.progress = 0;
        otaState.error = "";

        // Start filesystem update
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            otaState.status = "error";
            otaState.error = "Failed to start filesystem update";
            LOG_ERROR("OTA", "Update.begin(SPIFFS) failed");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            otaState.status = "error";
            otaState.error = "Write failed";
            LOG_ERROR("OTA", "Update.write() failed");
        } else {
            if (otaState.totalSize > 0) {
                otaState.currentSize += upload.currentSize;
                otaState.progress = (otaState.currentSize * 100) / otaState.totalSize;
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            otaState.status = "complete";
            otaState.progress = 100;
            LOG_INFO("OTA", ("Filesystem update complete, size: " + String(upload.totalSize)).c_str());
        } else {
            otaState.status = "error";
            otaState.error = "Update failed: " + String(Update.errorString());
            LOG_ERROR("OTA", ("Update failed: " + String(Update.errorString())).c_str());
        }
        otaState.inProgress = false;
    }
}

void WebServerManager::_handleFilesystemUpdate() {
    CORS_HEADERS();

    if (otaState.status == "complete") {
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Filesystem updated successfully. Rebooting...";

        String json;
        serializeJson(doc, json);
        _sendJSON(200, json);

        delay(1000);
        ESP.restart();
    } else if (otaState.status == "error") {
        _sendError(500, otaState.error);
        statusLED.setStatus(SYS_STATUS_ONLINE);
    } else {
        _sendError(400, "No update in progress");
    }
}

void WebServerManager::_handleGithubRelease() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    String repo = doc["repo"] | "";
    if (repo.length() == 0 || repo.indexOf('/') < 0) {
        _sendError(400, "Invalid repository format (use: owner/repo)");
        return;
    }

    // Check network
    if (!networkManager.isConnected()) {
        _sendError(503, "Network not connected");
        return;
    }

    LOG_INFO("OTA", ("Checking GitHub release for: " + repo).c_str());

    HTTPClient http;
    String url = "https://api.github.com/repos/" + repo + "/releases/latest";

    http.begin(url);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.addHeader("User-Agent", "ESP32-OTA");

    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();

        JsonDocument releaseDoc;
        DeserializationError err = deserializeJson(releaseDoc, payload);

        if (!err) {
            String tagName = releaseDoc["tag_name"] | "";
            String publishedAt = releaseDoc["published_at"] | "";
            String releaseBody = releaseDoc["body"] | "";

            // Extract version number
            String version = tagName;
            if (version.startsWith("v")) {
                version = version.substring(1);
            }

            // Compare with current version
            bool updateAvailable = (version != GATEWAY_VERSION);

            // Look for firmware.bin asset
            String firmwareUrl = "";
            String filesystemUrl = "";

            JsonArray assets = releaseDoc["assets"].as<JsonArray>();
            for (JsonObject asset : assets) {
                String name = asset["name"] | "";
                String downloadUrl = asset["browser_download_url"] | "";

                if (name == "firmware.bin") {
                    firmwareUrl = downloadUrl;
                } else if (name == "littlefs.bin" || name == "spiffs.bin") {
                    filesystemUrl = downloadUrl;
                }
            }

            JsonDocument response;
            response["success"] = true;
            response["release"]["version"] = tagName;
            response["release"]["date"] = publishedAt.substring(0, 10);
            response["release"]["notes"] = releaseBody.substring(0, 500);
            response["release"]["update_available"] = updateAvailable;
            response["release"]["firmware_url"] = firmwareUrl;
            response["release"]["filesystem_url"] = filesystemUrl;

            String json;
            serializeJson(response, json);
            _sendJSON(200, json);
        } else {
            _sendError(500, "Failed to parse release info");
        }
    } else if (httpCode == 404) {
        _sendError(404, "Repository or release not found");
    } else {
        _sendError(500, "GitHub API error: " + String(httpCode));
    }

    http.end();
}

void WebServerManager::_handleGithubUpdate() {
    CORS_HEADERS();

    String body = _getRequestBody();
    if (body.length() == 0) {
        _sendError(400, "Empty request body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(400, "Invalid JSON");
        return;
    }

    String repo = doc["repo"] | "";
    String type = doc["type"] | "firmware";  // firmware or filesystem

    if (repo.length() == 0) {
        _sendError(400, "Missing repository");
        return;
    }

    if (!networkManager.isConnected()) {
        _sendError(503, "Network not connected");
        return;
    }

    // Start the update process in a separate task
    otaState.inProgress = true;
    otaState.status = "downloading";
    otaState.progress = 0;
    otaState.error = "";

    statusLED.setStatus(SYS_STATUS_OTA_UPDATE);

    // Get release info and download URL
    HTTPClient http;
    String url = "https://api.github.com/repos/" + repo + "/releases/latest";

    http.begin(url);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.addHeader("User-Agent", "ESP32-OTA");

    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        http.end();

        JsonDocument releaseDoc;
        deserializeJson(releaseDoc, payload);

        String firmwareUrl = "";
        JsonArray assets = releaseDoc["assets"].as<JsonArray>();
        for (JsonObject asset : assets) {
            String name = asset["name"] | "";
            if (name == "firmware.bin") {
                firmwareUrl = asset["browser_download_url"] | "";
                break;
            }
        }

        if (firmwareUrl.length() == 0) {
            otaState.status = "error";
            otaState.error = "No firmware.bin found in release";
            otaState.inProgress = false;
            _sendError(404, "No firmware.bin found in release");
            return;
        }

        // Download and install firmware
        LOG_INFO("OTA", ("Downloading firmware from: " + firmwareUrl).c_str());

        HTTPClient httpFw;
        httpFw.begin(firmwareUrl);
        httpFw.addHeader("User-Agent", "ESP32-OTA");
        httpFw.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int fwCode = httpFw.GET();

        if (fwCode == 200) {
            int contentLength = httpFw.getSize();
            otaState.totalSize = contentLength;

            if (contentLength > 0) {
                bool canBegin = Update.begin(contentLength, U_FLASH);

                if (canBegin) {
                    otaState.status = "installing";
                    WiFiClient* stream = httpFw.getStreamPtr();

                    size_t written = 0;
                    uint8_t buf[1024];
                    while (httpFw.connected() && (written < contentLength)) {
                        size_t available = stream->available();
                        if (available) {
                            size_t toRead = (available > sizeof(buf)) ? sizeof(buf) : available;
                            size_t readBytes = stream->readBytes(buf, toRead);
                            size_t writtenBytes = Update.write(buf, readBytes);

                            if (writtenBytes != readBytes) {
                                otaState.status = "error";
                                otaState.error = "Write failed";
                                break;
                            }

                            written += writtenBytes;
                            otaState.progress = (written * 100) / contentLength;
                            otaState.currentSize = written;
                        }
                        delay(1);
                    }

                    if (Update.end()) {
                        if (Update.isFinished()) {
                            otaState.status = "complete";
                            otaState.progress = 100;
                            LOG_INFO("OTA", "GitHub update successful!");

                            JsonDocument resp;
                            resp["success"] = true;
                            resp["message"] = "Update successful. Rebooting...";
                            String json;
                            serializeJson(resp, json);
                            _sendJSON(200, json);

                            httpFw.end();
                            delay(1000);
                            ESP.restart();
                            return;
                        }
                    }
                    otaState.status = "error";
                    otaState.error = Update.errorString();
                } else {
                    otaState.status = "error";
                    otaState.error = "Not enough space";
                }
            } else {
                otaState.status = "error";
                otaState.error = "Invalid content length";
            }
        } else {
            otaState.status = "error";
            otaState.error = "Download failed: " + String(fwCode);
        }
        httpFw.end();
    } else {
        http.end();
        otaState.status = "error";
        otaState.error = "Failed to get release info";
    }

    otaState.inProgress = false;
    statusLED.setStatus(SYS_STATUS_ONLINE);
    _sendError(500, otaState.error);
}

void WebServerManager::_handleUpdateProgress() {
    CORS_HEADERS();

    JsonDocument doc;
    doc["status"] = otaState.status;
    doc["progress"] = otaState.progress;
    doc["error"] = otaState.error;
    doc["in_progress"] = otaState.inProgress;

    String json;
    serializeJson(doc, json);
    _sendJSON(200, json);
}
