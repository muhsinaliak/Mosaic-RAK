/**
 * @file node_firmware.ino
 * @brief Mosaic RAK - Node (Uç Cihaz) Firmware
 * @version 1.0.0
 *
 * RAK3172 (STM32WLE5) tabanlı 2 kanallı röle kontrol modülü.
 * LoRa P2P üzerinden Gateway ile haberleşir.
 *
 * Donanım:
 * - RAK3172 Modülü
 * - 2x Röle (GPIO kontrolü)
 *
 * Modlar:
 * - Discovery: ID atanana kadar HELLO yayını
 * - Normal: Komut dinleme ve status bildirimi
 */

#include "protocol.h"

// ============================================================================
// PIN TANIMLARI (RAK3172 Evaluation Board)
// ============================================================================

#define RELAY_1_PIN         PA0     // Röle 1 kontrol pini
#define RELAY_2_PIN         PA1     // Röle 2 kontrol pini
#define LED_STATUS_PIN      PA8     // Durum LED'i (varsa)
#define LED_LORA_PIN        PA9     // LoRa aktivite LED'i (varsa)

// Röle aktif seviyesi (HIGH = aktif veya LOW = aktif)
#define RELAY_ACTIVE_LEVEL  HIGH

// ============================================================================
// LORA P2P VARSAYILAN AYARLARI
// ============================================================================

#define LORA_FREQUENCY      868000000   // 868 MHz (EU)
#define LORA_SF             7           // Spreading Factor
#define LORA_BW             0           // 0 = 125kHz
#define LORA_CR             1           // 1 = 4/5
#define LORA_PREAMBLE       8           // Preamble length
#define LORA_TX_POWER       14          // TX Power (dBm)

// ============================================================================
// FLASH/NVM ADRESLERI (User Data Area)
// ============================================================================

#define NVM_MAGIC_ADDR          0       // Magic byte adresi
#define NVM_NODE_ID_ADDR        4       // Node ID adresi
#define NVM_LORA_FREQ_ADDR      8       // Frekans (4 byte)
#define NVM_LORA_SF_ADDR        12      // SF
#define NVM_LORA_BW_ADDR        16      // BW

#define NVM_MAGIC_VALUE         0xA5    // Geçerli veri işareti

// ============================================================================
// ZAMAN AYARLARI
// ============================================================================

#define STATUS_REPORT_PERIOD    60000   // Status raporu (ms)
#define HEARTBEAT_PERIOD        30000   // Heartbeat (ms)

// ============================================================================
// GLOBAL DEĞİŞKENLER
// ============================================================================

// Cihaz durumu
typedef enum {
    STATE_INIT,
    STATE_DISCOVERY,
    STATE_NORMAL
} DeviceState_t;

DeviceState_t   deviceState = STATE_INIT;
uint8_t         nodeID = NODE_ID_UNASSIGNED;
uint8_t         macAddr[MAC_ADDR_LEN];
uint8_t         relayStatus = 0x00;
uint16_t        seqNum = 0;
uint32_t        startTime = 0;

// Zamanlayıcılar
uint32_t        lastHelloTime = 0;
uint32_t        lastStatusTime = 0;
uint32_t        lastHeartbeatTime = 0;
uint32_t        nextHelloInterval = 0;
uint32_t        discoveryStartTime = 0;

// LoRa ayarları
uint32_t        loraFrequency = LORA_FREQUENCY;
uint8_t         loraSF = LORA_SF;
uint8_t         loraBW = LORA_BW;
uint8_t         loraCR = LORA_CR;
int8_t          loraTxPower = LORA_TX_POWER;

// Alınan paket bilgisi
int8_t          lastRSSI = 0;
int8_t          lastSNR = 0;

// RX Buffer
uint8_t         rxBuffer[MAX_PACKET_SIZE];
uint16_t        rxLen = 0;
volatile bool   rxDone = false;

// TX durumu
volatile bool   txBusy = false;

// ============================================================================
// FONKSİYON PROTOTİPLERİ
// ============================================================================

void initHardware(void);
void initLoRa(void);
void loadConfig(void);
void saveConfig(void);
void generateMAC(void);
void setRelay(uint8_t relayNum, bool state);
void applyRelayBitmap(uint8_t bitmap);
uint32_t getRandomInterval(void);
void sendHelloPacket(void);
void sendAckPacket(uint8_t ackType, uint8_t status);
void sendDataPacket(void);
void sendHeartbeat(void);
void processRxPacket(void);
void handleWelcome(WelcomePacket_t* pkt);
void handleCommand(CommandPacket_t* pkt);
void handleConfig(ConfigPacket_t* pkt);
void blinkLED(uint8_t pin, uint8_t count, uint16_t delayMs);
void updateLoRaConfig(void);
void handleDiscoveryMode(uint32_t currentTime);
void handleNormalMode(uint32_t currentTime);

// ============================================================================
// LORA CALLBACK FONKSİYONLARI (RUI3 API)
// ============================================================================

/**
 * @brief LoRa P2P RX tamamlandı callback
 * @param data Alınan veri yapısı (Buffer, BufferSize, Rssi, Snr)
 * @note RUI3 API'de tip adı "rui_lora_p2p_revc" (typo: revc)
 */
void onLoRaRxDone(rui_lora_p2p_revc data) {
    if (data.BufferSize > 0 && data.BufferSize <= MAX_PACKET_SIZE) {
        memcpy(rxBuffer, data.Buffer, data.BufferSize);
        rxLen = data.BufferSize;
        lastRSSI = data.Rssi;
        lastSNR = data.Snr;
        rxDone = true;
    }
}

/**
 * @brief LoRa P2P TX tamamlandı callback
 */
void onLoRaTxDone(void) {
    txBusy = false;
    digitalWrite(LED_LORA_PIN, LOW);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Seri port (debug)
    Serial.begin(115200);
    delay(2000);
    Serial.println("=================================");
    Serial.println("Mosaic RAK - Node Firmware v1.0.0");
    Serial.println("=================================");

    // Random seed
    randomSeed(analogRead(0) ^ millis());

    // Donanım başlatma
    initHardware();

    // MAC adresi oluştur
    generateMAC();
    Serial.print("MAC Address: ");
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        if (i > 0) Serial.print(":");
        Serial.printf("%02X", macAddr[i]);
    }
    Serial.println();

    // Flash'tan konfigürasyon yükle
    loadConfig();

    // LoRa P2P başlat
    initLoRa();

    // Başlangıç zamanı
    startTime = millis();

    // Durum belirleme
    if (nodeID == NODE_ID_UNASSIGNED) {
        deviceState = STATE_DISCOVERY;
        discoveryStartTime = millis();
        nextHelloInterval = getRandomInterval();
        Serial.println("Mode: DISCOVERY");
        blinkLED(LED_STATUS_PIN, 3, 100);
    } else {
        deviceState = STATE_NORMAL;
        Serial.printf("Mode: NORMAL (ID: %d)\n", nodeID);
        blinkLED(LED_STATUS_PIN, 1, 500);
    }

    // RX modunu başlat
    api.lora.precv(65534);  // Sürekli dinleme

    Serial.println("Setup complete. Starting main loop...");
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    uint32_t currentTime = millis();

    // Gelen paket kontrolü
    if (rxDone) {
        rxDone = false;
        processRxPacket();
        // RX modunu tekrar başlat
        api.lora.precv(65534);
    }

    // Durum makinesine göre işlem
    switch (deviceState) {
        case STATE_DISCOVERY:
            handleDiscoveryMode(currentTime);
            break;

        case STATE_NORMAL:
            handleNormalMode(currentTime);
            break;

        default:
            deviceState = STATE_INIT;
            break;
    }

    // Kısa bekleme
    delay(10);
}

// ============================================================================
// DISCOVERY MODU
// ============================================================================

void handleDiscoveryMode(uint32_t currentTime) {
    // HELLO gönderme zamanı geldi mi?
    if (currentTime - lastHelloTime >= nextHelloInterval) {
        // TX meşgul değilse gönder
        if (!txBusy) {
            sendHelloPacket();
            lastHelloTime = currentTime;
            nextHelloInterval = getRandomInterval();

            Serial.printf("HELLO sent. Next in %lu ms\n", nextHelloInterval);

            // LED yanıp sönsün
            blinkLED(LED_LORA_PIN, 1, 50);
        }
    }
}

// ============================================================================
// NORMAL MOD
// ============================================================================

void handleNormalMode(uint32_t currentTime) {
    // Periyodik status raporu
    if (currentTime - lastStatusTime >= STATUS_REPORT_PERIOD) {
        if (!txBusy) {
            sendDataPacket();
            lastStatusTime = currentTime;
            Serial.println("STATUS sent");
        }
    }

    // Heartbeat
    if (currentTime - lastHeartbeatTime >= HEARTBEAT_PERIOD) {
        if (!txBusy) {
            sendHeartbeat();
            lastHeartbeatTime = currentTime;
        }
    }
}

// ============================================================================
// PAKET GÖNDERİM FONKSİYONLARI
// ============================================================================

/**
 * @brief HELLO paketi gönder (Discovery modu)
 */
void sendHelloPacket(void) {
    HelloPacket_t pkt;
    pkt.type = PKG_HELLO;
    memcpy(pkt.macAddr, macAddr, MAC_ADDR_LEN);
    pkt.deviceType = DEV_TYPE_RELAY_2CH;
    pkt.fwVersion = 0x10;  // v1.0

    // RX modunu durdur
    api.lora.precv(0);
    delay(10);

    txBusy = true;
    digitalWrite(LED_LORA_PIN, HIGH);

    if (!api.lora.psend(sizeof(HelloPacket_t), (uint8_t*)&pkt)) {
        Serial.println("HELLO send failed!");
        txBusy = false;
        digitalWrite(LED_LORA_PIN, LOW);
    }

    delay(100);  // TX tamamlanmasını bekle

    // RX modunu tekrar başlat
    api.lora.precv(65534);
    txBusy = false;
}

/**
 * @brief ACK paketi gönder
 */
void sendAckPacket(uint8_t ackType, uint8_t status) {
    AckPacket_t pkt;
    pkt.type = PKG_ACK;
    pkt.nodeID = nodeID;
    pkt.ackType = ackType;
    pkt.status = status;

    // RX modunu durdur
    api.lora.precv(0);
    delay(10);

    txBusy = true;
    digitalWrite(LED_LORA_PIN, HIGH);

    if (!api.lora.psend(sizeof(AckPacket_t), (uint8_t*)&pkt)) {
        Serial.println("ACK send failed!");
    }

    delay(100);

    // RX modunu tekrar başlat
    api.lora.precv(65534);
    txBusy = false;
    digitalWrite(LED_LORA_PIN, LOW);
}

/**
 * @brief DATA paketi gönder (Status raporu)
 */
void sendDataPacket(void) {
    DataPacket_t pkt;
    pkt.type = PKG_DATA;
    pkt.nodeID = nodeID;
    pkt.relayStatus = relayStatus;
    pkt.rssi = lastRSSI;
    pkt.snr = lastSNR;
    pkt.batteryLevel = 0xFF;  // Harici güç
    pkt.uptime = (millis() - startTime) / 1000;

    // RX modunu durdur
    api.lora.precv(0);
    delay(10);

    txBusy = true;
    digitalWrite(LED_LORA_PIN, HIGH);

    if (!api.lora.psend(sizeof(DataPacket_t), (uint8_t*)&pkt)) {
        Serial.println("DATA send failed!");
    }

    delay(100);

    // RX modunu tekrar başlat
    api.lora.precv(65534);
    txBusy = false;
    digitalWrite(LED_LORA_PIN, LOW);
}

/**
 * @brief Heartbeat paketi gönder
 */
void sendHeartbeat(void) {
    HeartbeatPacket_t pkt;
    pkt.type = PKG_HEARTBEAT;
    pkt.nodeID = nodeID;
    pkt.relayStatus = relayStatus;
    pkt.errorFlags = 0x00;
    pkt.seqNum = seqNum++;

    // RX modunu durdur
    api.lora.precv(0);
    delay(10);

    txBusy = true;
    digitalWrite(LED_LORA_PIN, HIGH);

    if (!api.lora.psend(sizeof(HeartbeatPacket_t), (uint8_t*)&pkt)) {
        Serial.println("HEARTBEAT send failed!");
    }

    delay(100);

    // RX modunu tekrar başlat
    api.lora.precv(65534);
    txBusy = false;
    digitalWrite(LED_LORA_PIN, LOW);
}

// ============================================================================
// PAKET İŞLEME FONKSİYONLARI
// ============================================================================

/**
 * @brief Gelen paketi işle
 */
void processRxPacket(void) {
    if (rxLen < 1) return;

    uint8_t pktType = rxBuffer[0];

    Serial.printf("RX: Type=0x%02X, Len=%d, RSSI=%d, SNR=%d\n",
                  pktType, rxLen, lastRSSI, lastSNR);

    switch (pktType) {
        case PKG_WELCOME:
            if (rxLen >= sizeof(WelcomePacket_t)) {
                handleWelcome((WelcomePacket_t*)rxBuffer);
            }
            break;

        case PKG_CMD:
            if (rxLen >= sizeof(CommandPacket_t) &&
                deviceState == STATE_NORMAL) {
                handleCommand((CommandPacket_t*)rxBuffer);
            }
            break;

        case PKG_CONFIG:
            if (rxLen >= sizeof(ConfigPacket_t) &&
                deviceState == STATE_NORMAL) {
                handleConfig((ConfigPacket_t*)rxBuffer);
            }
            break;

        default:
            // Bilinmeyen paket tipi - sessizce yoksay
            break;
    }
}

/**
 * @brief WELCOME paketini işle
 */
void handleWelcome(WelcomePacket_t* pkt) {
    // MAC adresi kontrolü
    if (memcmp(pkt->targetMac, macAddr, MAC_ADDR_LEN) != 0) {
        Serial.println("WELCOME: MAC mismatch, ignoring");
        return;
    }

    // Discovery modunda mıyız?
    if (deviceState != STATE_DISCOVERY) {
        Serial.println("WELCOME: Not in discovery mode");
        return;
    }

    // ID'yi kaydet
    nodeID = pkt->assignedID;
    Serial.printf("WELCOME received! Assigned ID: %d\n", nodeID);

    // Flash'a kaydet
    saveConfig();

    // ACK gönder
    delay(50);  // Kısa bekleme
    sendAckPacket(PKG_WELCOME, ERR_NONE);
    Serial.println("ACK sent");

    // Normal moda geç
    deviceState = STATE_NORMAL;
    lastStatusTime = millis();
    lastHeartbeatTime = millis();

    // Başarı göstergesi
    blinkLED(LED_STATUS_PIN, 5, 100);

    Serial.println("Switched to NORMAL mode");
}

/**
 * @brief Komut paketini işle
 */
void handleCommand(CommandPacket_t* pkt) {
    // Hedef ID kontrolü
    if (pkt->targetID != nodeID && pkt->targetID != NODE_ID_BROADCAST) {
        return;
    }

    Serial.printf("CMD: type=%d, param1=0x%02X, param2=0x%02X\n",
                  pkt->cmdType, pkt->param1, pkt->param2);

    uint8_t errCode = ERR_NONE;

    switch (pkt->cmdType) {
        case CMD_RELAY_SET:
            // Röle durumunu ayarla (param1 = bitmap)
            applyRelayBitmap(pkt->param1);
            Serial.printf("Relays set to: 0x%02X\n", relayStatus);
            break;

        case CMD_RELAY_TOGGLE:
            // Belirli röleyi toggle et (param1 = röle numarası)
            if (pkt->param1 >= 1 && pkt->param1 <= 2) {
                TGL_RELAY_STATE(relayStatus, pkt->param1);
                setRelay(pkt->param1, GET_RELAY_STATE(relayStatus, pkt->param1));
                Serial.printf("Relay %d toggled\n", pkt->param1);
            } else {
                errCode = ERR_INVALID_PARAM;
            }
            break;

        case CMD_RESET:
            Serial.println("Resetting...");
            sendAckPacket(PKG_CMD, ERR_NONE);
            delay(100);
            api.system.reboot();
            break;

        case CMD_FACTORY_RESET:
            Serial.println("Factory reset...");
            nodeID = NODE_ID_UNASSIGNED;
            relayStatus = 0x00;
            applyRelayBitmap(0x00);

            // Flash'ı temizle
            {
                uint8_t zero = 0x00;
                api.system.flash.set(NVM_MAGIC_ADDR, &zero, 1);
                api.system.flash.set(NVM_NODE_ID_ADDR, &zero, 1);
            }

            sendAckPacket(PKG_CMD, ERR_NONE);
            delay(100);
            api.system.reboot();
            break;

        case CMD_REQUEST_STATUS:
            // Hemen status paketi gönder
            sendDataPacket();
            return;  // ACK yerine DATA gönderildi

        default:
            errCode = ERR_INVALID_CMD;
            break;
    }

    // ACK gönder
    delay(30);
    sendAckPacket(PKG_CMD, errCode);
}

/**
 * @brief Konfigürasyon paketini işle
 */
void handleConfig(ConfigPacket_t* pkt) {
    // Hedef ID kontrolü
    if (pkt->targetID != nodeID && pkt->targetID != NODE_ID_BROADCAST) {
        return;
    }

    Serial.println("CONFIG received");
    Serial.printf("  Freq: %lu Hz\n", pkt->frequency);
    Serial.printf("  SF: %d, BW: %d, CR: %d\n", pkt->sf, pkt->bw, pkt->cr);
    Serial.printf("  TX Power: %d dBm\n", pkt->txPower);

    // Parametreleri güncelle
    if (pkt->frequency >= 863000000 && pkt->frequency <= 870000000) {
        loraFrequency = pkt->frequency;
    }
    if (pkt->sf >= 7 && pkt->sf <= 12) {
        loraSF = pkt->sf;
    }
    if (pkt->bw <= 2) {
        loraBW = pkt->bw;
    }
    if (pkt->cr >= 1 && pkt->cr <= 4) {
        loraCR = pkt->cr;
    }
    if (pkt->txPower >= -4 && pkt->txPower <= 22) {
        loraTxPower = pkt->txPower;
    }

    // ACK gönder (eski ayarlarla)
    sendAckPacket(PKG_CONFIG, ERR_NONE);
    delay(100);

    // Yeni LoRa ayarlarını uygula
    updateLoRaConfig();

    // Flash'a kaydet
    saveConfig();

    Serial.println("LoRa config updated");
}

// ============================================================================
// DONANIM FONKSİYONLARI
// ============================================================================

/**
 * @brief Donanım başlatma
 */
void initHardware(void) {
    // Röle pinleri
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);

    // Röleleri kapat
    digitalWrite(RELAY_1_PIN, !RELAY_ACTIVE_LEVEL);
    digitalWrite(RELAY_2_PIN, !RELAY_ACTIVE_LEVEL);

    // LED pinleri
    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(LED_LORA_PIN, OUTPUT);

    digitalWrite(LED_STATUS_PIN, LOW);
    digitalWrite(LED_LORA_PIN, LOW);

    Serial.println("Hardware initialized");
}

/**
 * @brief LoRa P2P modunu başlat
 */
void initLoRa(void) {
    Serial.println("Initializing LoRa P2P...");

    // LoRa modunu P2P olarak ayarla
    if (!api.lora.nwm.set()) {
        Serial.println("Failed to set P2P mode!");
    }

    delay(300);

    // P2P parametrelerini ayarla
    api.lora.pfreq.set(loraFrequency);
    api.lora.psf.set(loraSF);
    api.lora.pbw.set(loraBW);
    api.lora.pcr.set(loraCR);
    api.lora.ptp.set(loraTxPower);
    api.lora.ppl.set(LORA_PREAMBLE);

    // Callback'leri ayarla
    api.lora.registerPRecvCallback(onLoRaRxDone);
    api.lora.registerPSendCallback(onLoRaTxDone);

    Serial.printf("LoRa P2P configured:\n");
    Serial.printf("  Freq: %lu Hz\n", loraFrequency);
    Serial.printf("  SF: %d, BW: %d, CR: %d\n", loraSF, loraBW, loraCR);
    Serial.printf("  TX Power: %d dBm\n", loraTxPower);
}

/**
 * @brief LoRa ayarlarını güncelle
 */
void updateLoRaConfig(void) {
    // Dinlemeyi durdur
    api.lora.precv(0);

    delay(50);

    // Yeni parametreleri uygula
    api.lora.pfreq.set(loraFrequency);
    api.lora.psf.set(loraSF);
    api.lora.pbw.set(loraBW);
    api.lora.pcr.set(loraCR);
    api.lora.ptp.set(loraTxPower);

    delay(50);

    // Dinlemeyi tekrar başlat
    api.lora.precv(65534);
}

/**
 * @brief Röle durumunu ayarla
 */
void setRelay(uint8_t relayNum, bool state) {
    uint8_t pin;

    switch (relayNum) {
        case 1: pin = RELAY_1_PIN; break;
        case 2: pin = RELAY_2_PIN; break;
        default: return;
    }

    digitalWrite(pin, state ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);

    if (state) {
        SET_RELAY_STATE(relayStatus, relayNum);
    } else {
        CLR_RELAY_STATE(relayStatus, relayNum);
    }
}

/**
 * @brief Röle bitmap'ini uygula
 */
void applyRelayBitmap(uint8_t bitmap) {
    setRelay(1, (bitmap & RELAY_1_BIT) != 0);
    setRelay(2, (bitmap & RELAY_2_BIT) != 0);
    relayStatus = bitmap & 0x03;  // Sadece 2 röle
}

/**
 * @brief Benzersiz MAC adresi oluştur (chip ID'den)
 */
void generateMAC(void) {
    // RAK3172 unique device ID'sinden MAC oluştur
    String chipId = api.system.chipId.get();

    // OUI (Organizationally Unique Identifier)
    macAddr[0] = 0x70;  // RAK
    macAddr[1] = 0xB3;
    macAddr[2] = 0xD5;

    // Son 3 byte: chipId string'inden hash oluştur
    uint32_t hash = 0;
    for (unsigned int i = 0; i < chipId.length(); i++) {
        hash = hash * 31 + chipId[i];
    }

    macAddr[3] = (hash >> 16) & 0xFF;
    macAddr[4] = (hash >> 8) & 0xFF;
    macAddr[5] = hash & 0xFF;
}

/**
 * @brief Discovery için rastgele aralık hesapla
 */
uint32_t getRandomInterval(void) {
    // 10-30 saniye arası rastgele
    return DISCOVERY_MIN_INTERVAL +
           random(DISCOVERY_MAX_INTERVAL - DISCOVERY_MIN_INTERVAL);
}

// ============================================================================
// FLASH/NVM FONKSİYONLARI
// ============================================================================

/**
 * @brief Konfigürasyonu Flash'tan yükle
 */
void loadConfig(void) {
    uint8_t magic = 0;
    api.system.flash.get(NVM_MAGIC_ADDR, &magic, 1);

    if (magic != NVM_MAGIC_VALUE) {
        Serial.println("No valid config in Flash");
        nodeID = NODE_ID_UNASSIGNED;
        return;
    }

    // Node ID
    api.system.flash.get(NVM_NODE_ID_ADDR, &nodeID, 1);

    // LoRa ayarları
    api.system.flash.get(NVM_LORA_FREQ_ADDR, (uint8_t*)&loraFrequency, 4);
    api.system.flash.get(NVM_LORA_SF_ADDR, &loraSF, 1);
    api.system.flash.get(NVM_LORA_BW_ADDR, &loraBW, 1);

    // Geçerlilik kontrolü
    if (loraFrequency < 863000000 || loraFrequency > 870000000) {
        loraFrequency = LORA_FREQUENCY;
    }
    if (loraSF < 7 || loraSF > 12) {
        loraSF = LORA_SF;
    }
    if (loraBW > 2) {
        loraBW = LORA_BW;
    }

    Serial.printf("Config loaded: ID=%d, Freq=%lu\n", nodeID, loraFrequency);
}

/**
 * @brief Konfigürasyonu Flash'a kaydet
 */
void saveConfig(void) {
    uint8_t magic = NVM_MAGIC_VALUE;

    api.system.flash.set(NVM_MAGIC_ADDR, &magic, 1);
    api.system.flash.set(NVM_NODE_ID_ADDR, &nodeID, 1);
    api.system.flash.set(NVM_LORA_FREQ_ADDR, (uint8_t*)&loraFrequency, 4);
    api.system.flash.set(NVM_LORA_SF_ADDR, &loraSF, 1);
    api.system.flash.set(NVM_LORA_BW_ADDR, &loraBW, 1);

    Serial.println("Config saved to Flash");
}

// ============================================================================
// YARDIMCI FONKSİYONLAR
// ============================================================================

/**
 * @brief LED yanıp söndür
 */
void blinkLED(uint8_t pin, uint8_t count, uint16_t delayMs) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(pin, HIGH);
        delay(delayMs);
        digitalWrite(pin, LOW);
        if (i < count - 1) {
            delay(delayMs);
        }
    }
}
