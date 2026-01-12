/**
 * @file protocol.h
 * @brief Mosaic RAK - LoRa P2P Binary Protocol Definitions
 * @version 1.0.0
 *
 * Gateway ve Node arasındaki haberleşme protokolü.
 * Tüm yapılar little-endian ve packed formatındadır.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <string.h>  // memcpy, memcmp için

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PAKET TİPLERİ
// ============================================================================

typedef enum {
    PKG_HELLO       = 0x01,   // Node -> Gateway: Discovery broadcast
    PKG_WELCOME     = 0x02,   // Gateway -> Node: ID assignment
    PKG_ACK         = 0x03,   // Node -> Gateway: Acknowledgment
    PKG_DATA        = 0x04,   // Bidirectional: Data/Status packet
    PKG_CMD         = 0x05,   // Gateway -> Node: Command packet
    PKG_CONFIG      = 0x06,   // Gateway -> Node: LoRa config update
    PKG_HEARTBEAT   = 0x07,   // Node -> Gateway: Periodic heartbeat
} PacketType_t;

// ============================================================================
// CİHAZ TİPLERİ
// ============================================================================

typedef enum {
    DEV_TYPE_UNKNOWN    = 0x00,
    DEV_TYPE_RELAY_2CH  = 0x01,   // 2 Kanallı Röle Modülü
    DEV_TYPE_RELAY_4CH  = 0x02,   // 4 Kanallı Röle Modülü
    DEV_TYPE_SENSOR     = 0x03,   // Sensör Modülü
    DEV_TYPE_GATEWAY    = 0xFF,   // Gateway
} DeviceType_t;

// ============================================================================
// KOMUT TİPLERİ
// ============================================================================

typedef enum {
    CMD_RELAY_SET       = 0x01,   // Röle durumu ayarla (bitmap)
    CMD_RELAY_TOGGLE    = 0x02,   // Röle toggle
    CMD_RESET           = 0x03,   // Cihazı resetle
    CMD_FACTORY_RESET   = 0x04,   // Fabrika ayarlarına dön
    CMD_REQUEST_STATUS  = 0x05,   // Durum bilgisi iste
} CommandType_t;

// ============================================================================
// RÖLE DURUMU BİTMAP
// ============================================================================

#define RELAY_1_BIT     (1 << 0)  // Bit 0: Röle 1
#define RELAY_2_BIT     (1 << 1)  // Bit 1: Röle 2
#define RELAY_3_BIT     (1 << 2)  // Bit 2: Röle 3
#define RELAY_4_BIT     (1 << 3)  // Bit 3: Röle 4

// ============================================================================
// PAKET YAPILARI (PACKED)
// ============================================================================

/**
 * @brief Paket Header - Tüm paketlerin başında bulunur
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // Paket tipi (PacketType_t)
    uint8_t     version;        // Protokol versiyonu (0x01)
} PacketHeader_t;

/**
 * @brief Hello Paketi - Node'un kendini tanıtması
 * Direction: Node -> Gateway (Broadcast)
 * Size: 9 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_HELLO (0x01)
    uint8_t     macAddr[6];     // Node'un benzersiz MAC adresi
    uint8_t     deviceType;     // Cihaz tipi (DeviceType_t)
    uint8_t     fwVersion;      // Firmware versiyonu
} HelloPacket_t;

/**
 * @brief Welcome Paketi - Gateway'in Node'u kabul etmesi
 * Direction: Gateway -> Node
 * Size: 9 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_WELCOME (0x02)
    uint8_t     targetMac[6];   // Hedef Node'un MAC adresi
    uint8_t     assignedID;     // Atanan Node ID (1-254)
    uint8_t     reserved;       // Gelecek kullanım için
} WelcomePacket_t;

/**
 * @brief ACK Paketi - Onay mesajı
 * Direction: Node -> Gateway
 * Size: 4 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_ACK (0x03)
    uint8_t     nodeID;         // Node ID
    uint8_t     ackType;        // Onaylanan paket tipi
    uint8_t     status;         // 0: OK, diğer: hata kodu
} AckPacket_t;

/**
 * @brief Data Paketi - Durum/Veri bildirimi
 * Direction: Bidirectional
 * Size: 12 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_DATA (0x04)
    uint8_t     nodeID;         // Node ID
    uint8_t     relayStatus;    // Röle durumu bitmap
    int8_t      rssi;           // RSSI değeri (dBm)
    int8_t      snr;            // SNR değeri (dB)
    uint8_t     batteryLevel;   // Batarya seviyesi (0-100, 0xFF=harici güç)
    uint32_t    uptime;         // Çalışma süresi (saniye)
} DataPacket_t;

/**
 * @brief Command Paketi - Gateway'den Node'a komut
 * Direction: Gateway -> Node
 * Size: 5 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_CMD (0x05)
    uint8_t     targetID;       // Hedef Node ID (0xFF = broadcast)
    uint8_t     cmdType;        // Komut tipi (CommandType_t)
    uint8_t     param1;         // Parametre 1 (ör: röle bitmap)
    uint8_t     param2;         // Parametre 2
} CommandPacket_t;

/**
 * @brief Config Paketi - LoRa ayarları güncelleme
 * Direction: Gateway -> Node
 * Size: 12 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_CONFIG (0x06)
    uint8_t     targetID;       // Hedef Node ID (0xFF = broadcast)
    uint32_t    frequency;      // Frekans (Hz) - ör: 868000000
    uint8_t     sf;             // Spreading Factor (7-12)
    uint8_t     bw;             // Bandwidth (0:125, 1:250, 2:500 kHz)
    uint8_t     cr;             // Coding Rate (1:4/5, 2:4/6, 3:4/7, 4:4/8)
    int8_t      txPower;        // TX Gücü (dBm)
    uint16_t    preamble;       // Preamble uzunluğu
} ConfigPacket_t;

/**
 * @brief Heartbeat Paketi - Periyodik yaşam sinyali
 * Direction: Node -> Gateway
 * Size: 6 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t     type;           // PKG_HEARTBEAT (0x07)
    uint8_t     nodeID;         // Node ID
    uint8_t     relayStatus;    // Röle durumu bitmap
    uint8_t     errorFlags;     // Hata bayrakları
    uint16_t    seqNum;         // Sıra numarası
} HeartbeatPacket_t;

// ============================================================================
// PROTOKOL SABİTLERİ
// ============================================================================

#define PROTOCOL_VERSION        0x01
#define NODE_ID_UNASSIGNED      0x00
#define NODE_ID_BROADCAST       0xFF
#define GATEWAY_ID              0xFE

#define MAX_PACKET_SIZE         32
#define MAC_ADDR_LEN            6

// Discovery timing (milliseconds)
#define DISCOVERY_MIN_INTERVAL  10000   // 10 saniye
#define DISCOVERY_MAX_INTERVAL  30000   // 30 saniye
#define DISCOVERY_TIMEOUT       60000   // 60 saniye timeout

// Normal mode timing (milliseconds)
#define STATUS_REPORT_INTERVAL  60000   // 60 saniye
#define HEARTBEAT_INTERVAL      30000   // 30 saniye
#define CMD_RESPONSE_TIMEOUT    5000    // 5 saniye

// ============================================================================
// HATA KODLARI
// ============================================================================

typedef enum {
    ERR_NONE            = 0x00,
    ERR_INVALID_CMD     = 0x01,
    ERR_INVALID_PARAM   = 0x02,
    ERR_RELAY_FAULT     = 0x03,
    ERR_EEPROM_FAULT    = 0x04,
    ERR_LORA_FAULT      = 0x05,
} ErrorCode_t;

// ============================================================================
// YARDIMCI MAKROLAR
// ============================================================================

#define GET_RELAY_STATE(bitmap, relay_num)  (((bitmap) >> ((relay_num) - 1)) & 0x01)
#define SET_RELAY_STATE(bitmap, relay_num)  ((bitmap) |= (1 << ((relay_num) - 1)))
#define CLR_RELAY_STATE(bitmap, relay_num)  ((bitmap) &= ~(1 << ((relay_num) - 1)))
#define TGL_RELAY_STATE(bitmap, relay_num)  ((bitmap) ^= (1 << ((relay_num) - 1)))

#ifdef __cplusplus
}
#endif

#endif // PROTOCOL_H
