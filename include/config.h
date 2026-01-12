/**
 * @file config.h
 * @brief Mosaic RAK Gateway - Global Configuration
 * @version 1.0.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// FIRMWARE INFO
// ============================================================================

#ifndef GATEWAY_VERSION
#define GATEWAY_VERSION         "1.0.0"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME             "Mintyfi_LoRa_Gateway"
#endif

// ============================================================================
// PIN DEFINITIONS (W5500 Ethernet - Waveshare ESP32-S3-ETH)
// ============================================================================

#ifndef W5500_MOSI
#define W5500_MOSI              11      // GPIO11 = ETH_MOSI
#endif

#ifndef W5500_MISO
#define W5500_MISO              12      // GPIO12 = ETH_MISO
#endif

#ifndef W5500_CLK
#define W5500_CLK               13      // GPIO13 = ETH_CLK
#endif

#ifndef W5500_CS
#define W5500_CS                14      // GPIO14 = ETH_CS
#endif

#ifndef W5500_INT
#define W5500_INT               10      // GPIO10 = ETH_INT
#endif

#ifndef W5500_RST
#define W5500_RST               9       // GPIO9 = ETH_RST
#endif

// ============================================================================
// PIN DEFINITIONS (LED & Button)
// ============================================================================

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN            21
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN              48
#endif

#define NEOPIXEL_COUNT          1

// ============================================================================
// PIN DEFINITIONS (RAK3172 UART - LoRa Module)
// ============================================================================

#ifndef LORA_UART_TX
#define LORA_UART_TX            17      // ESP32-S3 TX -> RAK3172 RX
#endif

#ifndef LORA_UART_RX
#define LORA_UART_RX            18      // ESP32-S3 RX <- RAK3172 TX
#endif

#ifndef LORA_UART_BAUD
#define LORA_UART_BAUD          115200
#endif

// ============================================================================
// LORA P2P CONFIGURATION
// ============================================================================

#define LORA_FREQUENCY          868000000   // 868 MHz (EU)
#define LORA_SF                 7           // Spreading Factor
#define LORA_BW                 0           // 0 = 125kHz
#define LORA_CR                 1           // 1 = 4/5
#define LORA_TX_POWER           14          // dBm
#define LORA_PREAMBLE           8

// Timing
#define LORA_RX_TIMEOUT         100         // ms - non-blocking check
#define LORA_TX_TIMEOUT         1000        // ms
#define LORA_SCAN_DURATION      60000       // 60 saniye scan modu
#define LORA_PAIRING_TIMEOUT    10000       // 10 saniye eşleşme timeout

// Limits
#define MAX_NODES               32          // Maksimum kayıtlı node sayısı
#define MAX_DISCOVERED_NODES    16          // Scan sırasında maksimum bulunan

// ============================================================================
// WEB SERVER CONFIGURATION
// ============================================================================

#define WEB_SERVER_PORT         80
#define API_PREFIX              "/api"

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================

#ifndef AP_SSID_PREFIX
#define AP_SSID_PREFIX          "Mintyfi_LoRa_GW_"
#endif

#ifndef AP_PASSWORD
#define AP_PASSWORD             "mintyfi123"    // Minimum 8 karakter (WPA2)
#endif

#ifndef AP_IP
#define AP_IP                   "192.168.1.1"
#endif

// Timeouts (milliseconds)
#define NETWORK_CONNECT_TIMEOUT 60000   // 60 saniye bağlantı timeout
#define NETWORK_RETRY_INTERVAL  10000   // 10 saniye yeniden deneme
#define ETHERNET_DHCP_TIMEOUT   30000   // 30 saniye DHCP timeout

// ============================================================================
// MQTT CONFIGURATION
// ============================================================================

#define MQTT_DEFAULT_PORT       1883
#define MQTT_BUFFER_SIZE        1024
#define MQTT_KEEPALIVE          60
#define MQTT_RECONNECT_INTERVAL 5000    // 5 saniye

// Topic prefixes
#define MQTT_TOPIC_PREFIX       "mintyfi/gateway/"
#define MQTT_TOPIC_STATUS       "status"
#define MQTT_TOPIC_NODES        "nodes/"
#define MQTT_TOPIC_CMD          "cmd/"
#define MQTT_TOPIC_CONFIG       "config/"

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================

#define FACTORY_RESET_HOLD_TIME 10000   // 10 saniye basılı tutma

// ============================================================================
// LED COLORS (RGB)
// ============================================================================

#define LED_COLOR_OFF           0x000000
#define LED_COLOR_GREEN         0x00FF00    // Bağlantı OK
#define LED_COLOR_RED           0xFF0000    // Hata
#define LED_COLOR_BLUE          0x0000FF    // Scan/Discovery
#define LED_COLOR_ORANGE        0xFF8000    // Reset/Config modu
#define LED_COLOR_YELLOW        0xFFFF00    // Uyarı
#define LED_COLOR_PURPLE        0xFF00FF    // AP Modu
#define LED_COLOR_CYAN          0x00FFFF    // MQTT bağlanıyor
#define LED_COLOR_WHITE         0xFFFFFF    // Boot

// ============================================================================
// FILE SYSTEM PATHS
// ============================================================================

#define CONFIG_FILE_PATH        "/config.json"
#define NODES_FILE_PATH         "/nodes.json"
#define LOG_FILE_PATH           "/logs/system.log"

// ============================================================================
// SYSTEM STATUS ENUM
// ============================================================================

typedef enum {
    SYS_STATUS_BOOT,            // Başlatılıyor
    SYS_STATUS_ETH_CONNECTING,  // Ethernet bağlanıyor
    SYS_STATUS_WIFI_CONNECTING, // WiFi bağlanıyor
    SYS_STATUS_AP_MODE,         // AP modunda
    SYS_STATUS_MQTT_CONNECTING, // MQTT bağlanıyor
    SYS_STATUS_ONLINE,          // Çevrimiçi - her şey OK
    SYS_STATUS_OFFLINE,         // Çevrimdışı - ağ yok
    SYS_STATUS_ERROR,           // Hata durumu
    SYS_STATUS_FACTORY_RESET,   // Fabrika ayarlarına dönülüyor
    SYS_STATUS_OTA_UPDATE,      // OTA güncelleme
} SystemStatus_t;

// ============================================================================
// NETWORK STATUS ENUM
// ============================================================================

typedef enum {
    NET_DISCONNECTED,
    NET_ETHERNET_CONNECTED,
    NET_WIFI_CONNECTED,
    NET_AP_MODE,
} NetworkStatus_t;

// ============================================================================
// DEBUG MACROS
// ============================================================================

#ifdef DEBUG_MODE
    #define DEBUG_PRINT(x)      Serial.print(x)
    #define DEBUG_PRINTLN(x)    Serial.println(x)
    #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#define LOG_INFO(tag, msg)      Serial.printf("[INFO][%s] %s\n", tag, msg)
#define LOG_WARN(tag, msg)      Serial.printf("[WARN][%s] %s\n", tag, msg)
#define LOG_ERROR(tag, msg)     Serial.printf("[ERROR][%s] %s\n", tag, msg)

#endif // CONFIG_H
