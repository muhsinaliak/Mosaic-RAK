/**
 * @file mqtt_client.h
 * @brief MQTT Client for Gateway Communication
 * @version 1.0.0
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "config.h"

// Callback tipleri
typedef std::function<void(bool connected)> MQTTConnectionCallback;
typedef std::function<void(const String& topic, const JsonDocument& payload)> MQTTMessageCallback;

class MQTTClient {
public:
    /**
     * @brief Constructor
     */
    MQTTClient();

    /**
     * @brief Initialize MQTT client
     * @param server MQTT broker address
     * @param port MQTT broker port
     * @param clientId Client ID (defaults to device ID)
     */
    bool begin(const String& server, uint16_t port = MQTT_DEFAULT_PORT, const String& clientId = "");

    /**
     * @brief Set authentication
     */
    void setAuth(const String& username, const String& password);

    /**
     * @brief Main update loop - call frequently
     */
    void update();

    /**
     * @brief Check if connected to broker
     */
    bool isConnected() const { return _connected; }

    /**
     * @brief Connect to broker
     */
    bool connect();

    /**
     * @brief Disconnect from broker
     */
    void disconnect();

    /**
     * @brief Publish JSON message
     * @param topic Topic (will be prefixed with gateway topic)
     * @param payload JSON document
     * @param retained Retain flag
     */
    bool publish(const String& topic, const JsonDocument& payload, bool retained = false);

    /**
     * @brief Publish string message
     */
    bool publish(const String& topic, const String& payload, bool retained = false);

    /**
     * @brief Subscribe to topic
     * @param topic Topic (will be prefixed with gateway topic)
     */
    bool subscribe(const String& topic);

    /**
     * @brief Unsubscribe from topic
     */
    bool unsubscribe(const String& topic);

    /**
     * @brief Set connection callback
     */
    void onConnection(MQTTConnectionCallback callback);

    /**
     * @brief Set message callback
     */
    void onMessage(MQTTMessageCallback callback);

    /**
     * @brief Publish gateway status
     */
    void publishStatus();

    /**
     * @brief Publish node data
     */
    void publishNodeData(uint8_t nodeId, const JsonDocument& data);

    /**
     * @brief Get full topic with prefix
     */
    String getFullTopic(const String& topic) const;

    /**
     * @brief Update server settings
     */
    void setServer(const String& server, uint16_t port);

    /**
     * @brief Get broker address
     */
    String getServer() const { return _server; }

    /**
     * @brief Get broker port
     */
    uint16_t getPort() const { return _port; }

    /**
     * @brief Get PubSubClient connection state
     * @return State code: -4=timeout, -3=lost, -2=failed, -1=disconnected, 0=connected,
     *         1=bad_protocol, 2=bad_client_id, 3=unavailable, 4=bad_credentials, 5=unauthorized
     */
    int getState() const;

    /**
     * @brief Internal message handler (public for static callback wrapper)
     */
    void handleMessage(char* topic, byte* payload, unsigned int length);

private:
    String              _server;
    uint16_t            _port;
    String              _clientId;
    String              _username;
    String              _password;

    bool                _connected;
    bool                _initialized;

    uint32_t            _lastReconnectAttempt;
    uint32_t            _lastStatusPublish;

    // Callbacks
    MQTTConnectionCallback  _connectionCallback;
    MQTTMessageCallback     _messageCallback;

    // Internal methods
    void                _tryReconnect();
    void                _subscribeToTopics();
    String              _generateClientId();
};

// Global instance
extern MQTTClient mqttClient;

#endif // MQTT_CLIENT_H
