/**
 * @file web_server.h
 * @brief Web Server & REST API Manager
 * @version 1.0.0
 *
 * REST API endpoints for frontend communication.
 * JSON-based request/response.
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <functional>
#include "config.h"

// ============================================================================
// API RESPONSE HELPERS
// ============================================================================

// CORS headers for API
#define CORS_HEADERS() \
    _server.sendHeader("Access-Control-Allow-Origin", "*"); \
    _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"); \
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type")

// ============================================================================
// WEB SERVER MANAGER SINIFI
// ============================================================================

class WebServerManager {
public:
    /**
     * @brief Constructor
     */
    WebServerManager();

    /**
     * @brief Initialize web server
     * @param port HTTP port (default 80)
     */
    bool begin(uint16_t port = WEB_SERVER_PORT);

    /**
     * @brief Non-blocking update - call frequently in loop()
     */
    void update();

    /**
     * @brief Stop web server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return _running; }

    /**
     * @brief Get WebServer instance for custom routes
     */
    WebServer& getServer() { return _server; }

private:
    WebServer       _server;
    bool            _running;

    // Route handlers
    void            _setupRoutes();
    void            _setupAPIRoutes();
    void            _setupStaticRoutes();

    // API Handlers
    void            _handleOptions();           // CORS preflight
    void            _handleStatus();            // GET /api/status
    void            _handleScan();              // GET /api/scan
    void            _handleScanResults();       // GET /api/scan-results
    void            _handleAddNode();           // POST /api/add
    void            _handleNodes();             // GET /api/nodes
    void            _handleNodeControl();       // POST /api/control
    void            _handleRemoveNode();        // DELETE /api/nodes/:id
    void            _handleConfig();            // GET /api/config
    void            _handleSaveConfig();        // POST /api/config
    void            _handleReboot();            // POST /api/reboot
    void            _handleFactoryReset();      // POST /api/factory-reset
    void            _handleWifiScan();          // GET /api/wifi-scan
    void            _handleMQTTPublish();       // POST /api/mqtt-publish
    void            _handleMQTTConnect();       // POST /api/mqtt-connect
    void            _handleWifiConnect();       // POST /api/wifi-connect
    void            _handleEthernetConnect();   // POST /api/ethernet-connect
    void            _handleEthernetStatus();    // GET /api/ethernet-status

    // OTA Update handlers
    void            _handleFirmwareUpdate();    // POST /api/update (complete)
    void            _handleFirmwareUpload();    // POST /api/update (upload handler)
    void            _handleFilesystemUpdate();  // POST /api/update-fs (complete)
    void            _handleFilesystemUpload();  // POST /api/update-fs (upload handler)
    void            _handleGithubRelease();     // POST /api/github-release
    void            _handleGithubUpdate();      // POST /api/github-update
    void            _handleUpdateProgress();    // GET /api/update-progress

    // Static file handlers
    void            _handleRoot();
    void            _handleNotFound();

    // Utility
    void            _sendJSON(int code, const String& json);
    void            _sendError(int code, const String& message);
    void            _sendSuccess(const String& message = "OK");
    String          _getRequestBody();
    void            _serveStaticFile(const String& path, const String& contentType);
};

// Global instance
extern WebServerManager webServerManager;

#endif // WEB_SERVER_H
