/**
 * @file mqtt_client.cpp
 * @brief MQTT Client Implementation
 * @version 1.0.0
 */

#include "mqtt_client.h"
#include "network_manager.h"
#include "config_manager.h"
#include "led_controller.h"

#include <WiFi.h>
#include <SPI.h>
#include <PubSubClient.h>

// Global instance
MQTTClient mqttClient;

// WiFi/Ethernet client
static WiFiClient wifiClient;
static PubSubClient* pubSubClient = nullptr;

// Access Ethernet client from network_manager.cpp
extern Client* getEthernetClient();

// Static callback wrapper
static MQTTClient* _instance = nullptr;

static void _mqttCallbackWrapper(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->handleMessage(topic, payload, length);
    }
}

MQTTClient::MQTTClient()
    : _port(MQTT_DEFAULT_PORT)
    , _connected(false)
    , _initialized(false)
    , _lastReconnectAttempt(0)
    , _lastStatusPublish(0)
    , _connectionCallback(nullptr)
    , _messageCallback(nullptr)
{
    _instance = this;
}

bool MQTTClient::begin(const String& server, uint16_t port, const String& clientId) {
    _server = server;
    _port = port;
    _clientId = clientId.length() > 0 ? clientId : _generateClientId();

    // Load from config if server is empty
    if (_server.length() == 0) {
        _server = configManager.getMQTTServer();
        _port = configManager.getMQTTPort();
        _username = configManager.getMQTTUser();
        _password = configManager.getMQTTPassword();
    }

    if (_server.length() == 0) {
        LOG_WARN("MQTT", "No MQTT server configured");
        return false;
    }

    // Initialize PubSubClient
    if (pubSubClient == nullptr) {
        pubSubClient = new PubSubClient();
    }

    pubSubClient->setServer(_server.c_str(), _port);
    pubSubClient->setBufferSize(MQTT_BUFFER_SIZE);
    pubSubClient->setKeepAlive(MQTT_KEEPALIVE);
    pubSubClient->setCallback(_mqttCallbackWrapper);

    _initialized = true;

    LOG_INFO("MQTT", ("Server: " + _server + ":" + String(_port)).c_str());
    LOG_INFO("MQTT", ("Client ID: " + _clientId).c_str());

    return true;
}

void MQTTClient::setAuth(const String& username, const String& password) {
    _username = username;
    _password = password;
}

void MQTTClient::update() {
    if (!_initialized || _server.length() == 0) return;

    // Check network connection
    if (!networkManager.isConnected()) {
        if (_connected) {
            _connected = false;
            if (_connectionCallback) {
                _connectionCallback(false);
            }
        }
        return;
    }

    // Process MQTT
    if (pubSubClient->connected()) {
        pubSubClient->loop();

        // Periodic status publish
        uint32_t now = millis();
        if (now - _lastStatusPublish >= 60000) {  // Every 60 seconds
            _lastStatusPublish = now;
            publishStatus();
        }
    } else {
        // Try reconnect
        _tryReconnect();
    }
}

bool MQTTClient::connect() {
    if (!_initialized || !networkManager.isConnected()) {
        return false;
    }

    statusLED.setStatus(SYS_STATUS_MQTT_CONNECTING);
    LOG_INFO("MQTT", "Connecting to broker...");

    bool result = false;

    // Select correct network client based on connection
    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("MQTT", "Using WiFi connection");
        pubSubClient->setClient(wifiClient);
    } else {
        LOG_INFO("MQTT", "Using Ethernet connection");
        Client* eth = getEthernetClient();
        if (eth) {
            pubSubClient->setClient(*eth);
        }
    }

    // Build Last Will message
    String willTopic = getFullTopic(MQTT_TOPIC_STATUS);
    String willMessage = "{\"online\":false}";

    if (_username.length() > 0) {
        result = pubSubClient->connect(
            _clientId.c_str(),
            _username.c_str(),
            _password.c_str(),
            willTopic.c_str(),
            0,      // QoS
            true,   // Retain
            willMessage.c_str()
        );
    } else {
        result = pubSubClient->connect(
            _clientId.c_str(),
            willTopic.c_str(),
            0,
            true,
            willMessage.c_str()
        );
    }

    if (result) {
        _connected = true;
        LOG_INFO("MQTT", "Connected to broker");

        // Subscribe to topics
        _subscribeToTopics();

        // Publish online status
        publishStatus();

        statusLED.setStatus(SYS_STATUS_ONLINE);

        if (_connectionCallback) {
            _connectionCallback(true);
        }
    } else {
        LOG_ERROR("MQTT", ("Connection failed, rc=" + String(pubSubClient->state())).c_str());
        statusLED.setStatus(SYS_STATUS_ERROR);
    }

    return result;
}

void MQTTClient::disconnect() {
    if (pubSubClient && pubSubClient->connected()) {
        // Publish offline status
        String topic = getFullTopic(MQTT_TOPIC_STATUS);
        pubSubClient->publish(topic.c_str(), "{\"online\":false}", true);

        pubSubClient->disconnect();
    }

    _connected = false;

    if (_connectionCallback) {
        _connectionCallback(false);
    }
}

bool MQTTClient::publish(const String& topic, const JsonDocument& payload, bool retained) {
    String jsonStr;
    serializeJson(payload, jsonStr);
    return publish(topic, jsonStr, retained);
}

bool MQTTClient::publish(const String& topic, const String& payload, bool retained) {
    if (!_connected || !pubSubClient) return false;

    String fullTopic = getFullTopic(topic);
    bool result = pubSubClient->publish(fullTopic.c_str(), payload.c_str(), retained);

    if (result) {
        DEBUG_PRINTF("[MQTT] Published: %s\n", fullTopic.c_str());
    } else {
        LOG_ERROR("MQTT", ("Publish failed: " + fullTopic).c_str());
    }

    return result;
}

bool MQTTClient::subscribe(const String& topic) {
    if (!_connected || !pubSubClient) return false;

    String fullTopic = getFullTopic(topic);
    bool result = pubSubClient->subscribe(fullTopic.c_str());

    if (result) {
        LOG_INFO("MQTT", ("Subscribed: " + fullTopic).c_str());
    }

    return result;
}

bool MQTTClient::unsubscribe(const String& topic) {
    if (!_connected || !pubSubClient) return false;

    String fullTopic = getFullTopic(topic);
    return pubSubClient->unsubscribe(fullTopic.c_str());
}

void MQTTClient::onConnection(MQTTConnectionCallback callback) {
    _connectionCallback = callback;
}

void MQTTClient::onMessage(MQTTMessageCallback callback) {
    _messageCallback = callback;
}

void MQTTClient::publishStatus() {
    JsonDocument doc;

    doc["online"] = true;
    doc["version"] = GATEWAY_VERSION;
    doc["ip"] = networkManager.getIP().toString();
    doc["connection"] = networkManager.getConnectionType();
    doc["rssi"] = networkManager.getRSSI();
    doc["uptime"] = millis() / 1000;
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_total"] = ESP.getHeapSize();

    publish(MQTT_TOPIC_STATUS, doc, true);
}

void MQTTClient::publishNodeData(uint8_t nodeId, const JsonDocument& data) {
    String topic = String(MQTT_TOPIC_NODES) + String(nodeId);
    publish(topic, data, false);
}

String MQTTClient::getFullTopic(const String& topic) const {
    return String(MQTT_TOPIC_PREFIX) + networkManager.getDeviceID() + "/" + topic;
}

void MQTTClient::setServer(const String& server, uint16_t port) {
    _server = server;
    _port = port;

    if (pubSubClient) {
        pubSubClient->setServer(_server.c_str(), _port);
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void MQTTClient::handleMessage(char* topic, byte* payload, unsigned int length) {
    // Parse topic
    String topicStr = String(topic);
    DEBUG_PRINTF("[MQTT] Received: %s\n", topic);

    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        LOG_ERROR("MQTT", "JSON parse error");
        return;
    }

    // Call user callback
    if (_messageCallback) {
        _messageCallback(topicStr, doc);
    }

    // Handle built-in commands
    String prefix = getFullTopic("");
    if (topicStr.startsWith(prefix)) {
        String subTopic = topicStr.substring(prefix.length());

        if (subTopic.startsWith("cmd/")) {
            // Command handling
            String cmd = doc["cmd"] | "";

            if (cmd == "restart") {
                LOG_INFO("MQTT", "Restart command received");
                delay(1000);
                ESP.restart();
            } else if (cmd == "factory_reset") {
                LOG_INFO("MQTT", "Factory reset command received");
                configManager.format();
                delay(1000);
                ESP.restart();
            } else if (cmd == "status") {
                publishStatus();
            }
        } else if (subTopic.startsWith("config/")) {
            // Config update handling
            if (doc.containsKey("mqtt_server")) {
                configManager.setMQTT(
                    doc["mqtt_server"] | "",
                    doc["mqtt_port"] | MQTT_DEFAULT_PORT,
                    doc["mqtt_user"] | "",
                    doc["mqtt_password"] | ""
                );
                configManager.save();
            }
        }
    }
}

void MQTTClient::_tryReconnect() {
    uint32_t now = millis();

    if (now - _lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
        _lastReconnectAttempt = now;

        if (_connected) {
            _connected = false;
            if (_connectionCallback) {
                _connectionCallback(false);
            }
        }

        connect();
    }
}

void MQTTClient::_subscribeToTopics() {
    // Subscribe to command topic
    subscribe(String(MQTT_TOPIC_CMD) + "#");

    // Subscribe to config topic
    subscribe(String(MQTT_TOPIC_CONFIG) + "#");

    // Subscribe to node commands
    subscribe(String(MQTT_TOPIC_NODES) + "+/cmd");
}

String MQTTClient::_generateClientId() {
    return "mintyfi_gw_" + networkManager.getDeviceID();
}

int MQTTClient::getState() const {
    if (pubSubClient) {
        return pubSubClient->state();
    }
    return -1;  // Disconnected
}
