/**
 * @file network_manager.cpp
 * @brief Network Manager Implementation
 * @version 1.0.0
 */

#include "network_manager.h"
#include "config_manager.h"
#include "led_controller.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

// Waveshare ETHClass2 for W5500 SPI Ethernet
#include <ETHClass2.h>

// Global instance
NetworkManager networkManager;

// DNS & Web Server for captive portal
static DNSServer dnsServer;
static WebServer webServer(80);
static const byte CAPTIVE_DNS_PORT = 53;

// Ethernet state tracking
static bool _ethConnected = false;
static bool _ethGotIP = false;

// WiFiClient works with both WiFi and ETH (shared TCP/IP stack)
static WiFiClient sharedClient;

// Helper to access Client from other modules
Client* getEthernetClient() {
    return &sharedClient;
}

// ETH Event Handler (compatible with ESP32 Arduino Core 2.x and 3.x)
void onEthEvent(WiFiEvent_t event) {
    switch (event) {
#ifdef ARDUINO_EVENT_ETH_START
        case ARDUINO_EVENT_ETH_START:
#else
        case SYSTEM_EVENT_ETH_START:
#endif
            {
                Serial.println("[ETH] Started");
                String hostname = configManager.getDeviceName();
                if (hostname.length() == 0) hostname = DEVICE_NAME;
                hostname.replace(" ", "-");
                hostname.replace("_", "-");
                ETH2.setHostname(hostname.c_str());
            }
            break;

#ifdef ARDUINO_EVENT_ETH_CONNECTED
        case ARDUINO_EVENT_ETH_CONNECTED:
#else
        case SYSTEM_EVENT_ETH_CONNECTED:
#endif
            Serial.println("[ETH] Link Up");
            _ethConnected = true;
            break;

#ifdef ARDUINO_EVENT_ETH_GOT_IP
        case ARDUINO_EVENT_ETH_GOT_IP:
#else
        case SYSTEM_EVENT_ETH_GOT_IP:
#endif
            Serial.printf("[ETH] Got IP: %s\n", ETH2.localIP().toString().c_str());
            Serial.printf("[ETH] MAC: %s, Speed: %dMbps, %s\n",
                         ETH2.macAddress().c_str(),
                         ETH2.linkSpeed(),
                         ETH2.fullDuplex() ? "Full Duplex" : "Half Duplex");
            _ethGotIP = true;
            break;

#ifdef ARDUINO_EVENT_ETH_DISCONNECTED
        case ARDUINO_EVENT_ETH_DISCONNECTED:
#else
        case SYSTEM_EVENT_ETH_DISCONNECTED:
#endif
            Serial.println("[ETH] Link Down");
            _ethConnected = false;
            _ethGotIP = false;
            break;

#ifdef ARDUINO_EVENT_ETH_STOP
        case ARDUINO_EVENT_ETH_STOP:
#else
        case SYSTEM_EVENT_ETH_STOP:
#endif
            Serial.println("[ETH] Stopped");
            _ethConnected = false;
            _ethGotIP = false;
            break;

        default:
            break;
    }
}

// Error page when SPA files are missing
static const char SPA_MISSING_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Error - Files Missing</title>
    <style>
        body { font-family: Arial, sans-serif; background: #1a1a2e; color: #fff; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
        .card { background: #fff; color: #333; padding: 30px; border-radius: 16px; max-width: 400px; text-align: center; }
        h1 { color: #e74c3c; margin-bottom: 15px; }
        p { margin: 10px 0; line-height: 1.6; }
        code { background: #f5f5f5; padding: 2px 6px; border-radius: 4px; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Web Interface Missing</h1>
        <p>The web interface files are not found in the device memory.</p>
        <p>Please upload the LittleFS filesystem using PlatformIO:</p>
        <p><code>pio run -t uploadfs</code></p>
        <p>This will upload the files from the <code>data/</code> folder.</p>
    </div>
</body>
</html>
)rawliteral";

NetworkManager::NetworkManager()
    : _status(NET_DISCONNECTED)
    , _lastConnectAttempt(0)
    , _connectStartTime(0)
    , _lastStatusCheck(0)
    , _ethernetInitialized(false)
    , _wifiInitialized(false)
    , _apModeActive(false)
    , _captivePortalActive(false)
    , _statusCallback(nullptr)
{
    memset(_mac, 0, 6);
}

bool NetworkManager::begin() {
    LOG_INFO("NET", "Initializing Network Manager...");

    // Generate MAC address
    _generateMAC();

    // Get configured connection mode
    ConnectionMode mode = configManager.getConnectionMode();
    LOG_INFO("NET", ("Connection mode: " + String(mode)).c_str());

    // Initialize hardware based on mode
    if (mode == CONN_MODE_ETHERNET) {
        // Ethernet mode selected
        if (_initEthernet()) {
            LOG_INFO("NET", "Ethernet initialized");
            if (_tryEthernetConnect()) {
                return true;
            }
        }
        // Ethernet failed, start AP mode
        LOG_WARN("NET", "Ethernet connection failed, starting AP mode");
        startAPMode();
        return true;
    }
    else if (mode == CONN_MODE_WIFI) {
        // WiFi mode selected
        if (_initWiFi()) {
            LOG_INFO("NET", "WiFi initialized");
            _loadCredentials();

            if (_savedSSID.length() > 0) {
                _connectStartTime = millis();
                if (_tryWiFiConnect()) {
                    return true;
                }
            }
        }
        // WiFi failed, start AP mode
        LOG_WARN("NET", "WiFi connection failed, starting AP mode");
        startAPMode();
        return true;
    }
    else {
        // CONN_MODE_NONE - Not configured, start AP mode directly
        LOG_INFO("NET", "No connection mode configured, starting AP mode");

        // Only initialize WiFi for AP mode
        // Don't initialize Ethernet - it interferes with WiFi connection attempts
        // Ethernet will be initialized when user explicitly selects it
        _initWiFi();

        startAPMode();
        return true;
    }
}

void NetworkManager::update() {
    uint32_t now = millis();

    // Handle captive portal if active
    if (_captivePortalActive) {
        _handleCaptivePortal();
    }

    // Periodic status check
    if (now - _lastStatusCheck >= 1000) {
        _lastStatusCheck = now;

        if (_status == NET_ETHERNET_CONNECTED) {
            // Check Ethernet link
            if (!ETH2.linkUp() || !_ethGotIP) {
                LOG_WARN("NET", "Ethernet link lost");
                _updateStatus(NET_DISCONNECTED);
                // Start AP mode for user access
                startAPMode();
            }
        } else if (_status == NET_WIFI_CONNECTED) {
            // Check WiFi connection
            if (WiFi.status() != WL_CONNECTED) {
                LOG_WARN("NET", "WiFi connection lost");
                _updateStatus(NET_DISCONNECTED);
                // Start AP mode for user access
                startAPMode();
            }
        }
    }

    // Auto-reconnect in AP mode - try to reconnect to configured network periodically
    if (_apModeActive && configManager.getConnectionMode() != CONN_MODE_NONE) {
        static uint32_t lastAPReconnectAttempt = 0;
        const uint32_t AP_RECONNECT_INTERVAL = 30000;  // Try every 30 seconds

        if (now - lastAPReconnectAttempt >= AP_RECONNECT_INTERVAL) {
            lastAPReconnectAttempt = now;
            ConnectionMode mode = configManager.getConnectionMode();

            if (mode == CONN_MODE_ETHERNET && _ethernetInitialized) {
                // Check if Ethernet cable is connected
                if (ETH2.linkUp()) {
                    LOG_INFO("NET", "AP Mode: Ethernet cable detected, trying to reconnect...");

                    // Stop AP mode first
                    _stopCaptivePortal();
                    WiFi.softAPdisconnect(true);
                    _apModeActive = false;

                    if (_tryEthernetConnect()) {
                        LOG_INFO("NET", "Auto-reconnected via Ethernet");
                        return;
                    } else {
                        // Failed, restart AP mode
                        LOG_WARN("NET", "Ethernet reconnect failed, restarting AP mode");
                        startAPMode();
                    }
                }
            }
            else if (mode == CONN_MODE_WIFI && _wifiInitialized && _savedSSID.length() > 0) {
                // Try to scan for saved network
                LOG_INFO("NET", "AP Mode: Scanning for saved WiFi network...");

                // Quick scan while in AP_STA mode
                WiFi.mode(WIFI_AP_STA);
                int n = WiFi.scanNetworks(false, false, false, 300);  // Fast scan

                bool networkFound = false;
                for (int i = 0; i < n; i++) {
                    if (WiFi.SSID(i) == _savedSSID) {
                        networkFound = true;
                        break;
                    }
                }
                WiFi.scanDelete();

                if (networkFound) {
                    LOG_INFO("NET", ("AP Mode: Network '" + _savedSSID + "' found, trying to reconnect...").c_str());

                    // Stop AP mode first
                    _stopCaptivePortal();
                    WiFi.softAPdisconnect(true);
                    WiFi.mode(WIFI_STA);
                    _apModeActive = false;

                    if (_tryWiFiConnect()) {
                        LOG_INFO("NET", "Auto-reconnected via WiFi");
                        return;
                    } else {
                        // Failed, restart AP mode
                        LOG_WARN("NET", "WiFi reconnect failed, restarting AP mode");
                        startAPMode();
                    }
                } else {
                    // Stay in AP mode
                    WiFi.mode(WIFI_AP);
                }
            }
        }
    }

    // Retry connection if disconnected (not in AP mode)
    if (_status == NET_DISCONNECTED && !_apModeActive) {
        if (now - _lastConnectAttempt >= NETWORK_RETRY_INTERVAL) {
            _lastConnectAttempt = now;
            LOG_INFO("NET", "Retrying connection...");

            // Try Ethernet first
            if (_ethernetInitialized && _tryEthernetConnect()) {
                return;
            }

            // Then try WiFi
            if (_wifiInitialized && _savedSSID.length() > 0) {
                _tryWiFiConnect();
            }
        }
    }

    // Check connect timeout during initial connection
    if (_connectStartTime > 0 && _status == NET_DISCONNECTED) {
        if (now - _connectStartTime >= NETWORK_CONNECT_TIMEOUT) {
            LOG_WARN("NET", "Connection timeout, starting AP mode");
            _connectStartTime = 0;
            startAPMode();
        }
    }
}

bool NetworkManager::isConnected() const {
    return (_status == NET_ETHERNET_CONNECTED || _status == NET_WIFI_CONNECTED);
}

String NetworkManager::getDeviceID() const {
    char id[16];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X",
             _mac[2], _mac[3], _mac[4], _mac[5]);
    return String(id);
}

void NetworkManager::startAPMode() {
    if (_apModeActive) return;

    LOG_INFO("NET", "Starting AP Mode...");

    // Disconnect any existing connection
    WiFi.disconnect(true);
    delay(100);

    // Generate AP SSID
    String apSSID = _generateAPSSID();

    // Configure AP
    IPAddress apIP(192, 168, 1, 1);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, gateway, subnet);

    // Start AP with password (must be 8+ chars for WPA2)
    bool apStarted = WiFi.softAP(apSSID.c_str(), AP_PASSWORD);

    _apModeActive = true;
    _currentIP = apIP;

    Serial.printf("[NET] AP Started: %s\n", apStarted ? "YES" : "NO");
    Serial.printf("[NET] AP SSID: %s\n", apSSID.c_str());
    Serial.printf("[NET] AP Password: %s\n", AP_PASSWORD);
    LOG_INFO("NET", ("AP IP: " + apIP.toString()).c_str());

    // Start captive portal
    _startCaptivePortal();

    _updateStatus(NET_AP_MODE);
}

void NetworkManager::stopAPMode() {
    if (!_apModeActive) return;

    LOG_INFO("NET", "Stopping AP Mode...");

    _stopCaptivePortal();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    _apModeActive = false;
    _updateStatus(NET_DISCONNECTED);

    // Try to connect
    _connectStartTime = millis();
}

void NetworkManager::reconnect() {
    LOG_INFO("NET", "Reconnecting...");

    if (_apModeActive) {
        stopAPMode();
    }

    _updateStatus(NET_DISCONNECTED);
    _connectStartTime = millis();
    _lastConnectAttempt = 0;
}

void NetworkManager::onStatusChange(NetworkCallback callback) {
    _statusCallback = callback;
}

bool NetworkManager::saveWiFiCredentials(const String& ssid, const String& password) {
    _savedSSID = ssid;
    _savedPassword = password;

    configManager.setWiFi(ssid, password);
    return configManager.save();
}

void NetworkManager::clearCredentials() {
    _savedSSID = "";
    _savedPassword = "";

    configManager.setWiFi("", "");
    configManager.save();
}

bool NetworkManager::isEthernetCableConnected() {
    if (!_ethernetInitialized) return false;
    return ETH2.linkUp();
}

bool NetworkManager::isEthernetConnected() const {
    return _ethGotIP;
}

IPAddress NetworkManager::getEthernetIP() const {
    if (_ethGotIP) {
        return ETH2.localIP();
    }
    return IPAddress(0, 0, 0, 0);
}

int32_t NetworkManager::getRSSI() const {
    if (_status == NET_WIFI_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

String NetworkManager::getConnectionType() const {
    switch (_status) {
        case NET_ETHERNET_CONNECTED: return "Ethernet";
        case NET_WIFI_CONNECTED: return "WiFi";
        case NET_AP_MODE: return "AP";
        default: return "None";
    }
}

bool NetworkManager::initEthernet() {
    if (_ethernetInitialized) {
        return true;  // Already initialized
    }
    return _initEthernet();
}

// ============================================================================
// Private Methods
// ============================================================================

// Helper function to validate static IP configuration
static bool isValidStaticIP(const char* ip) {
    if (ip == nullptr || strlen(ip) == 0) return false;

    IPAddress addr;
    if (!addr.fromString(ip)) return false;

    // Check if IP is not 0.0.0.0 (invalid)
    if (addr[0] == 0 && addr[1] == 0 && addr[2] == 0 && addr[3] == 0) return false;

    // Check if IP is not 255.255.255.255 (broadcast)
    if (addr[0] == 255 && addr[1] == 255 && addr[2] == 255 && addr[3] == 255) return false;

    return true;
}

bool NetworkManager::_initEthernet() {
    LOG_INFO("NET", "Initializing W5500 Ethernet (ETHClass2)...");
    Serial.printf("[NET] Pins: MOSI=%d, MISO=%d, CLK=%d, CS=%d, RST=%d, INT=%d\n",
                  W5500_MOSI, W5500_MISO, W5500_CLK, W5500_CS, W5500_RST, W5500_INT);

    // Register ETH event handler
    WiFi.onEvent(onEthEvent);

    // Initialize ETH2 with W5500 using ESP-IDF SPI driver
    // ETH2.begin(type, phy_addr, cs, irq, rst, spi_host, sck, miso, mosi, spi_freq_mhz)
    bool result = ETH2.begin(ETH_PHY_W5500, 1, W5500_CS, W5500_INT, W5500_RST,
                             SPI2_HOST, W5500_CLK, W5500_MISO, W5500_MOSI, 20);

    if (result) {
        LOG_INFO("NET", "W5500 ETH2.begin() successful!");

        // Set hostname for router identification (use device name from config)
        String hostname = configManager.getDeviceName();
        if (hostname.length() == 0) hostname = DEVICE_NAME;
        hostname.replace(" ", "-");  // Spaces not allowed in hostname
        hostname.replace("_", "-");
        ETH2.setHostname(hostname.c_str());
        LOG_INFO("NET", ("ETH Hostname: " + hostname).c_str());

        _ethernetInitialized = true;
        return true;
    }

    LOG_ERROR("NET", "W5500 ETH2.begin() failed!");
    return false;
}

bool NetworkManager::_initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    // Set hostname for router identification (use device name from config)
    String hostname = configManager.getDeviceName();
    if (hostname.length() == 0) hostname = DEVICE_NAME;
    hostname.replace(" ", "-");  // Spaces not allowed in hostname
    hostname.replace("_", "-");
    WiFi.setHostname(hostname.c_str());
    LOG_INFO("NET", ("WiFi Hostname: " + hostname).c_str());

    _wifiInitialized = true;
    return true;
}

bool NetworkManager::_tryEthernetConnect() {
    if (!_ethernetInitialized) return false;

    statusLED.setStatus(SYS_STATUS_ETH_CONNECTING);
    LOG_INFO("NET", "Waiting for Ethernet connection...");

    // Wait for link to come up (W5500 needs time to detect cable)
    uint32_t linkWaitStart = millis();
    while (!ETH2.linkUp() && (millis() - linkWaitStart < 3000)) {
        delay(100);
        statusLED.update();
    }

    // Check if link is up
    if (!ETH2.linkUp()) {
        LOG_WARN("NET", "Ethernet cable not connected");
        return false;
    }

    // Configure static IP if enabled and valid
    const GatewayConfig& config = configManager.getConfig();
    if (config.use_static_ip && isValidStaticIP(config.static_ip)) {
        IPAddress ip, gateway, subnet, dns;

        // Parse and validate all IP addresses
        if (ip.fromString(config.static_ip) &&
            gateway.fromString(config.gateway) &&
            subnet.fromString(config.subnet) &&
            dns.fromString(config.dns)) {

            LOG_INFO("NET", ("Ethernet static IP: " + String(config.static_ip)).c_str());
            LOG_INFO("NET", ("  Gateway: " + String(config.gateway)).c_str());
            LOG_INFO("NET", ("  Subnet: " + String(config.subnet)).c_str());
            LOG_INFO("NET", ("  DNS: " + String(config.dns)).c_str());

            // Apply static IP configuration BEFORE waiting for DHCP
            if (!ETH2.config(ip, gateway, subnet, dns)) {
                LOG_ERROR("NET", "Failed to configure static IP, falling back to DHCP");
            }

            // Re-set hostname after ETH2.config() (may be cleared)
            String hostname = configManager.getDeviceName();
            if (hostname.length() == 0) hostname = DEVICE_NAME;
            hostname.replace(" ", "-");
            hostname.replace("_", "-");
            ETH2.setHostname(hostname.c_str());
            LOG_INFO("NET", ("ETH Hostname: " + hostname).c_str());
        } else {
            LOG_WARN("NET", "Invalid static IP configuration, using DHCP");
        }
    } else {
        LOG_INFO("NET", "Ethernet using DHCP...");
    }

    // Wait for DHCP (IP address via event handler)
    uint32_t startTime = millis();
    uint32_t lastDebug = 0;
    while (!_ethGotIP && (millis() - startTime < ETHERNET_DHCP_TIMEOUT)) {
        delay(100);
        statusLED.update();

        // Debug output every 5 seconds
        if (millis() - lastDebug >= 5000) {
            lastDebug = millis();
            IPAddress ip = ETH2.localIP();
            Serial.printf("[NET] DHCP wait... IP=%s, ethGotIP=%d\n",
                         ip.toString().c_str(), _ethGotIP ? 1 : 0);

            // Check if we got IP but event didn't fire
            if (ip[0] != 0 && !_ethGotIP) {
                Serial.println("[NET] Got IP without event, setting flag");
                _ethGotIP = true;
            }
        }
    }

    if (!_ethGotIP) {
        // Last check - maybe we got IP but event didn't fire
        IPAddress ip = ETH2.localIP();
        if (ip[0] != 0) {
            Serial.println("[NET] IP detected at timeout, proceeding");
            _ethGotIP = true;
        } else {
            LOG_ERROR("NET", "DHCP timeout");
            return false;
        }
    }

    _currentIP = ETH2.localIP();
    LOG_INFO("NET", ("Ethernet IP: " + _currentIP.toString()).c_str());

    _updateStatus(NET_ETHERNET_CONNECTED);
    return true;
}

bool NetworkManager::_tryWiFiConnect() {
    if (!_wifiInitialized || _savedSSID.length() == 0) return false;

    statusLED.setStatus(SYS_STATUS_WIFI_CONNECTING);
    LOG_INFO("NET", ("Connecting to WiFi: " + _savedSSID).c_str());

    // Configure static IP if enabled and valid
    const GatewayConfig& config = configManager.getConfig();
    if (config.use_static_ip && isValidStaticIP(config.static_ip)) {
        IPAddress ip, gateway, subnet, dns;

        // Parse and validate all IP addresses
        if (ip.fromString(config.static_ip) &&
            gateway.fromString(config.gateway) &&
            subnet.fromString(config.subnet) &&
            dns.fromString(config.dns)) {

            LOG_INFO("NET", ("WiFi static IP: " + String(config.static_ip)).c_str());
            LOG_INFO("NET", ("  Gateway: " + String(config.gateway)).c_str());
            LOG_INFO("NET", ("  Subnet: " + String(config.subnet)).c_str());
            LOG_INFO("NET", ("  DNS: " + String(config.dns)).c_str());

            // Apply static IP configuration BEFORE WiFi.begin()
            if (!WiFi.config(ip, gateway, subnet, dns)) {
                LOG_ERROR("NET", "Failed to configure WiFi static IP, using DHCP");
            }

            // Re-set hostname after WiFi.config() (ESP32 clears it)
            String hostname = configManager.getDeviceName();
            if (hostname.length() == 0) hostname = DEVICE_NAME;
            hostname.replace(" ", "-");
            hostname.replace("_", "-");
            WiFi.setHostname(hostname.c_str());
            LOG_INFO("NET", ("WiFi Hostname: " + hostname).c_str());
        } else {
            LOG_WARN("NET", "Invalid WiFi static IP configuration, using DHCP");
        }
    } else {
        LOG_INFO("NET", "WiFi using DHCP...");
    }

    WiFi.begin(_savedSSID.c_str(), _savedPassword.c_str());

    // Wait for connection (non-blocking check done in update())
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        statusLED.update();

        if (millis() - startTime > 15000) {  // 15 second timeout for single attempt
            LOG_WARN("NET", "WiFi connection timeout");
            return false;
        }
    }

    _currentIP = WiFi.localIP();
    LOG_INFO("NET", ("WiFi IP: " + _currentIP.toString()).c_str());

    _updateStatus(NET_WIFI_CONNECTED);
    return true;
}

void NetworkManager::_startCaptivePortal() {
    LOG_INFO("NET", "Starting Captive Portal with SPA...");

    // Start DNS server for captive portal
    dnsServer.start(CAPTIVE_DNS_PORT, "*", WiFi.softAPIP());

    // Serve SPA index.html from LittleFS
    webServer.on("/", HTTP_GET, []() {
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            webServer.streamFile(file, "text/html");
            file.close();
        } else {
            webServer.send(200, "text/html", SPA_MISSING_HTML);
        }
    });

    // Serve static files from LittleFS
    webServer.on("/style.css", HTTP_GET, []() {
        if (LittleFS.exists("/style.css")) {
            File file = LittleFS.open("/style.css", "r");
            webServer.streamFile(file, "text/css");
            file.close();
        } else {
            webServer.send(404, "text/plain", "Not found");
        }
    });

    webServer.on("/script.js", HTTP_GET, []() {
        if (LittleFS.exists("/script.js")) {
            File file = LittleFS.open("/script.js", "r");
            webServer.streamFile(file, "application/javascript");
            file.close();
        } else {
            webServer.send(404, "text/plain", "Not found");
        }
    });

    // API: WiFi scan for SPA
    webServer.on("/api/wifi-scan", HTTP_GET, []() {
        // Visual feedback - blue blinking during scan (3 blinks)
        statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);

        String json = "{\"networks\":[";
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n && i < 20; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i));
            json += ",\"channel\":" + String(WiFi.channel(i)) + ",\"encryption\":";
            json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
            json += "}";
        }
        json += "],\"count\":" + String(n > 0 ? n : 0) + "}";
        WiFi.scanDelete();

        // Restore AP mode LED
        statusLED.setStatus(SYS_STATUS_AP_MODE);

        webServer.send(200, "application/json", json);
    });

    // API: Status for SPA
    webServer.on("/api/status", HTTP_GET, []() {
        String json = "{";
        json += "\"version\":\"" + String(GATEWAY_VERSION) + "\",";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"heap_total\":" + String(ESP.getHeapSize()) + ",";
        json += "\"network\":{\"connected\":false,\"type\":\"AP\",\"ip\":\"192.168.1.1\"},";
        json += "\"mqtt\":{\"connected\":false},";
        json += "\"lora\":{\"scanning\":false,\"nodes_registered\":0,\"nodes_online\":0}";
        json += "}";
        webServer.send(200, "application/json", json);
    });

    // API: Config GET for SPA
    webServer.on("/api/config", HTTP_GET, []() {
        const GatewayConfig& cfg = configManager.getConfig();
        String json = "{";
        json += "\"connection_mode\":" + String(configManager.getConnectionMode()) + ",";
        json += "\"device_name\":\"" + configManager.getDeviceName() + "\",";
        json += "\"wifi_ssid\":\"" + configManager.getWiFiSSID() + "\",";
        json += "\"mqtt_server\":\"" + configManager.getMQTTServer() + "\",";
        json += "\"mqtt_port\":" + String(configManager.getMQTTPort()) + ",";
        json += "\"mqtt_user\":\"" + configManager.getMQTTUser() + "\",";
        json += "\"led_brightness\":" + String(cfg.led_brightness) + ",";
        // Static IP settings
        json += "\"use_static_ip\":" + String(cfg.use_static_ip ? "true" : "false") + ",";
        json += "\"static_ip\":\"" + String(cfg.static_ip) + "\",";
        json += "\"gateway\":\"" + String(cfg.gateway) + "\",";
        json += "\"subnet\":\"" + String(cfg.subnet) + "\",";
        json += "\"dns\":\"" + String(cfg.dns) + "\"";
        json += "}";
        webServer.send(200, "application/json", json);
    });

    // API: Config POST for SPA
    webServer.on("/api/config", HTTP_POST, []() {
        String body = webServer.arg("plain");
        if (body.length() > 0) {
            configManager.fromJSON(body);
            configManager.save();
        }
        webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved\",\"restart_required\":true}");
    });

    // API: Ethernet status for SPA
    webServer.on("/api/ethernet-status", HTTP_GET, []() {
        bool ethConnected = networkManager.isEthernetConnected();
        bool cableConnected = networkManager.isEthernetCableConnected();

        String json = "{";
        json += "\"cable_connected\":" + String(cableConnected ? "true" : "false") + ",";
        json += "\"connected\":" + String(ethConnected ? "true" : "false") + ",";

        if (ethConnected) {
            json += "\"ip\":\"" + networkManager.getEthernetIP().toString() + "\",";
            json += "\"success\":true,";
            json += "\"message\":\"Ethernet connected\"";
        } else if (cableConnected) {
            json += "\"ip\":\"\",";
            json += "\"success\":false,";
            json += "\"message\":\"Cable connected, waiting for DHCP\"";
        } else {
            json += "\"ip\":\"\",";
            json += "\"success\":false,";
            json += "\"message\":\"Ethernet cable not connected\"";
        }
        json += "}";

        webServer.send(200, "application/json", json);
    });

    // API: Reboot for SPA
    webServer.on("/api/reboot", HTTP_POST, []() {
        webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });

    // API: Ethernet connect - saves config and returns IP
    webServer.on("/api/ethernet-connect", HTTP_POST, []() {
        String body = webServer.arg("plain");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        // Save connection mode to Ethernet
        configManager.setConnectionMode(CONN_MODE_ETHERNET);

        // Handle static IP if provided
        bool useStaticIP = false;
        IPAddress staticIP, staticGateway, staticSubnet, staticDNS;

        if (doc.containsKey("use_static_ip")) {
            GatewayConfig& config = configManager.getConfig();
            config.use_static_ip = doc["use_static_ip"];
            useStaticIP = config.use_static_ip;

            if (doc.containsKey("static_ip")) strlcpy(config.static_ip, doc["static_ip"], sizeof(config.static_ip));
            if (doc.containsKey("gateway")) strlcpy(config.gateway, doc["gateway"], sizeof(config.gateway));
            if (doc.containsKey("subnet")) strlcpy(config.subnet, doc["subnet"], sizeof(config.subnet));
            if (doc.containsKey("dns")) strlcpy(config.dns, doc["dns"], sizeof(config.dns));

            // Parse static IP addresses for immediate use
            if (useStaticIP) {
                staticIP.fromString(config.static_ip);
                staticGateway.fromString(config.gateway);
                staticSubnet.fromString(config.subnet);
                staticDNS.fromString(config.dns);

                // Validate static IP (not 0.0.0.0)
                if (staticIP[0] == 0 && staticIP[1] == 0 && staticIP[2] == 0 && staticIP[3] == 0) {
                    useStaticIP = false;
                    LOG_WARN("NET", "Invalid static IP (0.0.0.0), using DHCP");
                }
            }
        }

        configManager.save();

        // Visual feedback
        statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);
        LOG_INFO("NET", "Initializing Ethernet connection...");

        // Initialize Ethernet if not already done
        if (!networkManager.isEthernetInitialized()) {
            networkManager.initEthernet();
        }

        // Apply static IP configuration immediately if enabled
        if (useStaticIP) {
            LOG_INFO("NET", ("Applying static IP: " + staticIP.toString()).c_str());
            LOG_INFO("NET", ("  Gateway: " + staticGateway.toString()).c_str());
            LOG_INFO("NET", ("  Subnet: " + staticSubnet.toString()).c_str());
            LOG_INFO("NET", ("  DNS: " + staticDNS.toString()).c_str());
            ETH2.config(staticIP, staticGateway, staticSubnet, staticDNS);
        } else {
            LOG_INFO("NET", "Using DHCP for Ethernet");
        }

        // Wait for Ethernet IP (max 15 seconds)
        uint32_t startTime = millis();
        IPAddress ethIP;

        while (millis() - startTime < 15000) {
            ethIP = ETH2.localIP();
            if (ethIP != IPAddress(0, 0, 0, 0)) {
                break;
            }
            delay(500);
        }

        if (ethIP != IPAddress(0, 0, 0, 0)) {
            String ip = ethIP.toString();
            LOG_INFO("NET", ("Ethernet IP: " + ip).c_str());

            String response = "{\"success\":true,\"ip\":\"" + ip + "\",\"message\":\"Ethernet connected\"}";
            webServer.send(200, "application/json", response);
        } else {
            LOG_WARN("NET", "Ethernet: No IP obtained - check cable");
            webServer.send(200, "application/json", "{\"success\":false,\"ip\":\"\",\"error\":\"No IP - check Ethernet cable\"}");
        }
    });

    // API: WiFi connect test - tries to connect and returns result
    webServer.on("/api/wifi-connect", HTTP_POST, []() {
        String body = webServer.arg("plain");
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error || !doc["ssid"].is<const char*>()) {
            webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
            return;
        }

        String ssid = doc["ssid"].as<String>();
        String password = doc["password"] | "";

        // Save credentials and set connection mode to WiFi
        networkManager.saveWiFiCredentials(ssid, password);
        configManager.setConnectionMode(CONN_MODE_WIFI);

        // Handle static IP if provided
        bool useStaticIP = false;
        IPAddress staticIP, staticGateway, staticSubnet, staticDNS;

        if (doc.containsKey("use_static_ip")) {
            GatewayConfig& config = configManager.getConfig();
            config.use_static_ip = doc["use_static_ip"];
            useStaticIP = config.use_static_ip;

            if (doc.containsKey("static_ip")) strlcpy(config.static_ip, doc["static_ip"], sizeof(config.static_ip));
            if (doc.containsKey("gateway")) strlcpy(config.gateway, doc["gateway"], sizeof(config.gateway));
            if (doc.containsKey("subnet")) strlcpy(config.subnet, doc["subnet"], sizeof(config.subnet));
            if (doc.containsKey("dns")) strlcpy(config.dns, doc["dns"], sizeof(config.dns));

            // Parse static IP addresses for immediate use
            if (useStaticIP) {
                staticIP.fromString(config.static_ip);
                staticGateway.fromString(config.gateway);
                staticSubnet.fromString(config.subnet);
                staticDNS.fromString(config.dns);

                // Validate static IP (not 0.0.0.0)
                if (staticIP[0] == 0 && staticIP[1] == 0 && staticIP[2] == 0 && staticIP[3] == 0) {
                    useStaticIP = false;
                    LOG_WARN("NET", "Invalid static IP (0.0.0.0), using DHCP");
                }
            }
        }

        configManager.save();

        // Save MQTT if provided
        if (doc["mqtt_server"].is<const char*>()) {
            configManager.setMQTT(
                doc["mqtt_server"].as<String>(),
                doc["mqtt_port"] | 1883,
                doc["mqtt_user"] | "",
                doc["mqtt_password"] | ""
            );
            configManager.save();
        }

        // Visual feedback - blue blinking during connection
        statusLED.blink(LED_COLOR_BLUE, 3, 150, 150);
        LOG_INFO("NET", ("Trying to connect to: " + ssid).c_str());

        // Keep AP running while trying STA connection
        WiFi.mode(WIFI_AP_STA);
        delay(100);

        // Apply static IP configuration BEFORE WiFi.begin() if enabled
        if (useStaticIP) {
            LOG_INFO("NET", ("Applying WiFi static IP: " + staticIP.toString()).c_str());
            LOG_INFO("NET", ("  Gateway: " + staticGateway.toString()).c_str());
            LOG_INFO("NET", ("  Subnet: " + staticSubnet.toString()).c_str());
            LOG_INFO("NET", ("  DNS: " + staticDNS.toString()).c_str());
            WiFi.config(staticIP, staticGateway, staticSubnet, staticDNS);
        } else {
            LOG_INFO("NET", "WiFi using DHCP");
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

        wl_status_t finalStatus = WiFi.status();
        if (finalStatus == WL_CONNECTED) {
            String newIP = WiFi.localIP().toString();
            LOG_INFO("NET", ("Connected! IP: " + newIP).c_str());

            String response = "{\"success\":true,\"ip\":\"" + newIP + "\",\"message\":\"Connected successfully\"}";
            webServer.send(200, "application/json", response);

            // Wait for response to be sent, then restart
            delay(1000);
            ESP.restart();
        } else {
            // Connection failed - stay in AP mode
            WiFi.disconnect(false);  // Don't turn off radio
            WiFi.mode(WIFI_AP);
            statusLED.setStatus(SYS_STATUS_AP_MODE);

            String errorMsg = "Connection failed";
            if (finalStatus == WL_NO_SSID_AVAIL) {
                errorMsg = "Network not found";
            } else if (finalStatus == WL_CONNECT_FAILED) {
                errorMsg = "Wrong password";
            } else if (finalStatus == WL_DISCONNECTED) {
                errorMsg = "Connection rejected";
            } else if (finalStatus == WL_IDLE_STATUS) {
                errorMsg = "WiFi not responding - try again";
            }

            LOG_WARN("NET", ("WiFi connect failed. Status: " + String(finalStatus) + " - " + errorMsg).c_str());
            String response = "{\"success\":false,\"error\":\"" + errorMsg + "\"}";
            webServer.send(200, "application/json", response);
        }
    });

    // Legacy scan endpoint (for fallback HTML)
    webServer.on("/scan", HTTP_GET, []() {
        String json = "[";
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        webServer.send(200, "application/json", json);
    });

    // Legacy save endpoint (for fallback HTML)
    webServer.on("/save", HTTP_POST, []() {
        String ssid = webServer.arg("ssid");
        String password = webServer.arg("password");
        String mqttServer = webServer.arg("mqtt_server");
        uint16_t mqttPort = webServer.arg("mqtt_port").toInt();
        String mqttUser = webServer.arg("mqtt_user");
        String mqttPass = webServer.arg("mqtt_pass");

        if (ssid.length() > 0) {
            networkManager.saveWiFiCredentials(ssid, password);
        }

        if (mqttServer.length() > 0) {
            configManager.setMQTT(mqttServer, mqttPort, mqttUser, mqttPass);
            configManager.save();
        }

        webServer.send(200, "text/html",
            "<html><body><h1>Saved!</h1><p>Gateway will restart...</p></body></html>");

        delay(1000);
        ESP.restart();
    });

    // Catch-all: redirect to root or serve SPA for routes
    webServer.onNotFound([]() {
        String uri = webServer.uri();

        // Check if it's a request for captive portal detection
        if (uri.indexOf("generate_204") >= 0 ||
            uri.indexOf("connecttest") >= 0 ||
            uri.indexOf("hotspot-detect") >= 0 ||
            uri.indexOf("ncsi.txt") >= 0) {
            webServer.sendHeader("Location", "http://192.168.1.1/", true);
            webServer.send(302, "text/plain", "");
            return;
        }

        // Return JSON error for API routes not available in AP mode
        if (uri.startsWith("/api/")) {
            webServer.sendHeader("Access-Control-Allow-Origin", "*");
            webServer.send(503, "application/json",
                "{\"success\":false,\"error\":\"This feature is not available in AP mode. Please connect to a network first.\"}");
            return;
        }

        // Serve index.html for SPA routes
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            webServer.streamFile(file, "text/html");
            file.close();
        } else {
            webServer.sendHeader("Location", "http://192.168.1.1/", true);
            webServer.send(302, "text/plain", "");
        }
    });

    webServer.begin();
    _captivePortalActive = true;
    LOG_INFO("NET", "Captive Portal ready - serving SPA from LittleFS");
}

void NetworkManager::_stopCaptivePortal() {
    if (!_captivePortalActive) return;

    dnsServer.stop();
    webServer.stop();
    _captivePortalActive = false;
}

void NetworkManager::_handleCaptivePortal() {
    dnsServer.processNextRequest();
    webServer.handleClient();
}

void NetworkManager::_updateStatus(NetworkStatus_t newStatus) {
    if (_status != newStatus) {
        _status = newStatus;

        // Update LED
        switch (_status) {
            case NET_ETHERNET_CONNECTED:
            case NET_WIFI_CONNECTED:
                statusLED.setStatus(SYS_STATUS_ONLINE);
                break;
            case NET_AP_MODE:
                statusLED.setStatus(SYS_STATUS_AP_MODE);
                break;
            default:
                statusLED.setStatus(SYS_STATUS_OFFLINE);
                break;
        }

        // Call callback
        if (_statusCallback) {
            _statusCallback(_status, _currentIP);
        }
    }
}

void NetworkManager::_loadCredentials() {
    _savedSSID = configManager.getWiFiSSID();
    _savedPassword = configManager.getWiFiPassword();

    if (_savedSSID.length() > 0) {
        LOG_INFO("NET", ("Loaded WiFi SSID: " + _savedSSID).c_str());
    }
}

void NetworkManager::_generateMAC() {
    // Use ESP32 default MAC and modify for uniqueness
    esp_read_mac(_mac, ESP_MAC_WIFI_STA);
    _macString = "";
    for (int i = 0; i < 6; i++) {
        if (i > 0) _macString += ":";
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", _mac[i]);
        _macString += hex;
    }
}

String NetworkManager::_generateAPSSID() {
    // Get last 4 hex digits of MAC
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", _mac[4], _mac[5]);
    return String(AP_SSID_PREFIX) + suffix;
}
