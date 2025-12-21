/**
 * ESPNowManager.cpp
 * 
 * Implementation der universellen ESP-NOW Kommunikationsklasse
 * mit TLV-Protokoll, Builder-Pattern und Parser
 * OHNE Worker-Thread - ESP-NOW ist bereits asynchron!
 */

#include "include/ESPNowManager.h"
#include <esp_wifi.h>

// ═══════════════════════════════════════════════════════════════════════════
// ESPNOWPACKET - BUILDER & PARSER
// ═══════════════════════════════════════════════════════════════════════════

ESPNowPacket::ESPNowPacket()
    : entryCount(0)
    , mainCmd(MainCmd::NONE)
    , dataLength(0)
    , writePos(2)  // Nach Header starten
    , valid(false)
{
    memset(buffer, 0, ESPNOW_MAX_PACKET_SIZE);
    memset(entries, 0, sizeof(entries));
}

ESPNowPacket::~ESPNowPacket() {
}

// ─────────────────────────────────────────────────────────────────────────────
// BUILDER
// ─────────────────────────────────────────────────────────────────────────────

ESPNowPacket& ESPNowPacket::begin(MainCmd cmd) {
    clear();
    mainCmd = cmd;
    buffer[0] = static_cast<uint8_t>(cmd);
    buffer[1] = 0;  // Total length (wird am Ende aktualisiert)
    writePos = 2;
    dataLength = 0;
    valid = true;
    return *this;
}

ESPNowPacket& ESPNowPacket::add(DataCmd dataCmd, const void* data, size_t len) {
    if (!valid) return *this;
    
    // Prüfen ob noch Platz
    // Benötigt: 2 Byte (SUB_CMD + LEN) + Daten
    if (writePos + 2 + len > ESPNOW_MAX_PACKET_SIZE) {
        DEBUG_PRINTLN("ESPNowPacket: ❌ Kein Platz mehr im Paket!");
        return *this;
    }
    
    // Prüfen ob noch Einträge frei
    if (entryCount >= MAX_ENTRIES) {
        DEBUG_PRINTLN("ESPNowPacket: ❌ Maximale Einträge erreicht!");
        return *this;
    }
    
    // Sub-CMD schreiben
    buffer[writePos++] = static_cast<uint8_t>(dataCmd);
    
    // Länge schreiben
    buffer[writePos++] = static_cast<uint8_t>(len);
    
    // Daten kopieren
    if (data && len > 0) {
        memcpy(&buffer[writePos], data, len);
    }
    
    // Entry speichern (für Parser)
    entries[entryCount].cmd = dataCmd;
    entries[entryCount].offset = writePos - 2;  // Position im Buffer
    entries[entryCount].length = len;
    entryCount++;
    
    writePos += len;
    dataLength = writePos - 2;
    
    // Total length im Header aktualisieren
    buffer[1] = static_cast<uint8_t>(dataLength);
    
    return *this;
}

ESPNowPacket& ESPNowPacket::addByte(DataCmd dataCmd, uint8_t value) {
    return add(dataCmd, &value, 1);
}

ESPNowPacket& ESPNowPacket::addInt8(DataCmd dataCmd, int8_t value) {
    return add(dataCmd, &value, 1);
}

ESPNowPacket& ESPNowPacket::addUInt16(DataCmd dataCmd, uint16_t value) {
    return add(dataCmd, &value, 2);
}

ESPNowPacket& ESPNowPacket::addInt16(DataCmd dataCmd, int16_t value) {
    return add(dataCmd, &value, 2);
}

ESPNowPacket& ESPNowPacket::addUInt32(DataCmd dataCmd, uint32_t value) {
    return add(dataCmd, &value, 4);
}

ESPNowPacket& ESPNowPacket::addInt32(DataCmd dataCmd, int32_t value) {
    return add(dataCmd, &value, 4);
}

ESPNowPacket& ESPNowPacket::addFloat(DataCmd dataCmd, float value) {
    return add(dataCmd, &value, 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// PARSER
// ─────────────────────────────────────────────────────────────────────────────

bool ESPNowPacket::parse(const uint8_t* rawData, size_t len) {
    clear();
    
    // Mindestlänge prüfen (MAIN_CMD + TOTAL_LEN)
    if (!rawData || len < 2) {
        DEBUG_PRINTLN("ESPNowPacket: ❌ Paket zu klein");
        return false;
    }
    
    // Header lesen
    mainCmd = static_cast<MainCmd>(rawData[0]);
    uint8_t totalLen = rawData[1];
    
    // Längenprüfung
    if (totalLen > len - 2) {
        DEBUG_PRINTF("ESPNowPacket: ❌ Ungültige Länge: %d > %d\n", totalLen, len - 2);
        return false;
    }
    
    // Buffer kopieren
    memcpy(buffer, rawData, len);
    dataLength = totalLen;
    
    // Sub-Einträge parsen
    size_t pos = 2;  // Nach Header
    while (pos + 2 <= 2 + totalLen) {
        // Sub-CMD und Länge lesen
        DataCmd subCmd = static_cast<DataCmd>(buffer[pos]);
        uint8_t subLen = buffer[pos + 1];
        
        // Prüfen ob Daten noch im Paket
        if (pos + 2 + subLen > 2 + totalLen) {
            DEBUG_PRINTLN("ESPNowPacket: ⚠️ Truncated sub-entry");
            break;
        }
        
        // Entry speichern
        if (entryCount < MAX_ENTRIES) {
            entries[entryCount].cmd = subCmd;
            entries[entryCount].offset = pos;  // Position im Buffer
            entries[entryCount].length = subLen;
            entryCount++;
        }
        
        pos += 2 + subLen;
    }
    
    valid = true;
    return true;
}

bool ESPNowPacket::has(DataCmd dataCmd) const {
    return findEntry(dataCmd) >= 0;
}

const uint8_t* ESPNowPacket::getData(DataCmd dataCmd, size_t* outLen) const {
    int idx = findEntry(dataCmd);
    if (idx < 0) {
        if (outLen) *outLen = 0;
        return nullptr;
    }
    
    if (outLen) *outLen = entries[idx].length;
    
    // Daten beginnen 2 Bytes nach Offset (nach SUB_CMD + LEN)
    return &buffer[entries[idx].offset + 2];
}

bool ESPNowPacket::getByte(DataCmd dataCmd, uint8_t& outValue) const {
    const uint8_t* data = get<uint8_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getInt8(DataCmd dataCmd, int8_t& outValue) const {
    const int8_t* data = get<int8_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getUInt16(DataCmd dataCmd, uint16_t& outValue) const {
    const uint16_t* data = get<uint16_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getInt16(DataCmd dataCmd, int16_t& outValue) const {
    const int16_t* data = get<int16_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getUInt32(DataCmd dataCmd, uint32_t& outValue) const {
    const uint32_t* data = get<uint32_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getInt32(DataCmd dataCmd, int32_t& outValue) const {
    const int32_t* data = get<int32_t>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

bool ESPNowPacket::getFloat(DataCmd dataCmd, float& outValue) const {
    const float* data = get<float>(dataCmd);
    if (data) {
        outValue = *data;
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// HELPER
// ─────────────────────────────────────────────────────────────────────────────

void ESPNowPacket::clear() {
    memset(buffer, 0, ESPNOW_MAX_PACKET_SIZE);
    memset(entries, 0, sizeof(entries));
    entryCount = 0;
    mainCmd = MainCmd::NONE;
    dataLength = 0;
    writePos = 2;
    valid = false;
}

int ESPNowPacket::findEntry(DataCmd cmd) const {
    for (int i = 0; i < entryCount; i++) {
        if (entries[i].cmd == cmd) {
            return i;
        }
    }
    return -1;
}

void ESPNowPacket::print() const {
    DEBUG_PRINTLN("\n─── ESPNowPacket ───────────────────────────");
    DEBUG_PRINTF("MainCmd:    0x%02X\n", static_cast<uint8_t>(mainCmd));
    DEBUG_PRINTF("DataLength: %d\n", dataLength);
    DEBUG_PRINTF("Entries:    %d\n", entryCount);
    DEBUG_PRINTF("Valid:      %s\n", valid ? "YES" : "NO");
    
    for (int i = 0; i < entryCount; i++) {
        DEBUG_PRINTF("  [%d] DataCmd=0x%02X, Len=%d, Offset=%d\n", 
                     i, 
                     static_cast<uint8_t>(entries[i].cmd),
                     entries[i].length,
                     entries[i].offset);
    }
    
    // Hex-Dump
    DEBUG_PRINT("Raw: ");
    for (size_t i = 0; i < getTotalLength() && i < 32; i++) {
        DEBUG_PRINTF("%02X ", buffer[i]);
    }
    if (getTotalLength() > 32) DEBUG_PRINT("...");
    DEBUG_PRINTLN("\n────────────────────────────────────────────");
}

// ═══════════════════════════════════════════════════════════════════════════
// ESPNOWMANAGER - HAUPTKLASSE
// ═══════════════════════════════════════════════════════════════════════════

ESPNowManager::ESPNowManager()
    : initialized(false)
    , wifiChannel(0)
    , maxPeersLimit(5)           // Default: 5 Peers
    , peersMutex(nullptr)
    , heartbeatEnabled(false)
    , heartbeatInterval(500)
    , timeoutMs(2000)
    , lastHeartbeatSent(0)
    , rxQueue(nullptr)
    , receiveCallback(nullptr)
    , sendCallback(nullptr)
{
    for (int i = 0; i < 12; i++) {
        eventCallbacks[i] = nullptr;
    }
}

ESPNowManager::~ESPNowManager() {
    end();
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowManager::begin(uint8_t channel) {
    if (initialized) {
        DEBUG_PRINTLN("ESPNowManager: Bereits initialisiert");
        return true;
    }

    DEBUG_PRINTLN("ESPNowManager: Initialisiere ESP-NOW...");

    // ═══════════════════════════════════════════════════════════════════════
    // FreeRTOS Ressourcen erstellen
    // ═══════════════════════════════════════════════════════════════════════
    
    // Mutex für Peer-Liste
    peersMutex = xSemaphoreCreateMutex();
    if (!peersMutex) {
        DEBUG_PRINTLN("ESPNowManager: ❌ Mutex erstellen fehlgeschlagen!");
        return false;
    }
    
    // RX-Queue erstellen (WiFi-ISR → Main-Thread)
    rxQueue = xQueueCreate(ESPNOW_RX_QUEUE_SIZE, sizeof(RxQueueItem));
    
    if (!rxQueue) {
        DEBUG_PRINTLN("ESPNowManager: ❌ RX-Queue erstellen fehlgeschlagen!");
        end();
        return false;
    }
    
    DEBUG_PRINTLN("ESPNowManager: ✅ RX-Queue erstellt");

    // ═══════════════════════════════════════════════════════════════════════
    // WiFi & ESP-NOW initialisieren
    // ═══════════════════════════════════════════════════════════════════════
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    if (channel > 0 && channel <= 14) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        wifiChannel = channel;
    }

    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
        DEBUG_PRINTF("ESPNowManager: ❌ esp_now_init() fehlgeschlagen: %d\n", result);
        end();
        return false;
    }

    // Callbacks registrieren
    esp_now_register_recv_cb(onDataRecvStatic);
    esp_now_register_send_cb(onDataSentStatic);

    initialized = true;

    DEBUG_PRINTLN("ESPNowManager: ✅ ESP-NOW initialisiert (OHNE Worker-Thread)");
    DEBUG_PRINTF("ESPNowManager: MAC: %s, Kanal: %d\n", getOwnMacString().c_str(), wifiChannel);

    return true;
}

void ESPNowManager::end() {
    if (!initialized) return;

    DEBUG_PRINTLN("ESPNowManager: Beende ESP-NOW...");
    
    // Peers entfernen
    removeAllPeers();
    
    // ESP-NOW deinitialisieren
    esp_now_deinit();
    
    // Queue löschen
    if (rxQueue) {
        vQueueDelete(rxQueue);
        rxQueue = nullptr;
    }
    
    // Mutex löschen
    if (peersMutex) {
        vSemaphoreDelete(peersMutex);
        peersMutex = nullptr;
    }
    
    initialized = false;
    DEBUG_PRINTLN("ESPNowManager: ✅ ESP-NOW beendet");
}

// ═══════════════════════════════════════════════════════════════════════════
// PEER-VERWALTUNG (Thread-safe mit Mutex)
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowManager::addPeer(const uint8_t* mac, bool encrypt) {
    if (!initialized || !mac) return false;

    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        DEBUG_PRINTLN("ESPNowManager: ❌ Mutex-Timeout bei addPeer");
        return false;
    }

    bool result = false;
    
    // Prüfen ob bereits vorhanden
    if (findPeerIndex(mac) >= 0) {
        DEBUG_PRINTF("ESPNowManager: Peer %s existiert bereits\n", macToString(mac).c_str());
        result = true;
    }
    else if (peers.size() >= maxPeersLimit) {
        DEBUG_PRINTF("ESPNowManager: ❌ User-Limit erreicht (%d/%d Peers)\n", peers.size(), maxPeersLimit);
    }
    else if (peers.size() >= ESPNOW_MAX_PEERS_LIMIT) {
        DEBUG_PRINTLN("ESPNowManager: ❌ Hardware-Limit erreicht!");
    }
    else {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = wifiChannel;
        peerInfo.encrypt = encrypt;

        esp_err_t espResult = esp_now_add_peer(&peerInfo);
        if (espResult != ESP_OK) {
            DEBUG_PRINTF("ESPNowManager: ❌ esp_now_add_peer() fehlgeschlagen: %d\n", espResult);
        }
        else {
            ESPNowPeer newPeer;
            memcpy(newPeer.mac, mac, 6);
            newPeer.connected = false;
            newPeer.lastSeen = 0;
            newPeer.packetsReceived = 0;
            newPeer.packetsSent = 0;
            newPeer.packetsLost = 0;
            newPeer.rssi = 0;

            peers.push_back(newPeer);
            result = true;

            DEBUG_PRINTF("ESPNowManager: ✅ Peer hinzugefügt: %s\n", macToString(mac).c_str());
        }
    }

    xSemaphoreGive(peersMutex);

    if (result) {
        ESPNowEventData eventData = {};
        eventData.event = ESPNowEvent::PEER_ADDED;
        memcpy(eventData.mac, mac, 6);
        triggerEvent(ESPNowEvent::PEER_ADDED, &eventData);
    }

    return result;
}

bool ESPNowManager::removePeer(const uint8_t* mac) {
    if (!initialized || !mac) return false;

    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    int index = findPeerIndex(mac);
    bool result = false;
    
    if (index >= 0) {
        esp_now_del_peer(mac);
        peers.erase(peers.begin() + index);
        result = true;
        DEBUG_PRINTF("ESPNowManager: ✅ Peer entfernt: %s\n", macToString(mac).c_str());
    }

    xSemaphoreGive(peersMutex);

    if (result) {
        ESPNowEventData eventData = {};
        eventData.event = ESPNowEvent::PEER_REMOVED;
        memcpy(eventData.mac, mac, 6);
        triggerEvent(ESPNowEvent::PEER_REMOVED, &eventData);
    }

    return result;
}

void ESPNowManager::removeAllPeers() {
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    
    while (!peers.empty()) {
        esp_now_del_peer(peers[0].mac);
        peers.erase(peers.begin());
    }
    
    xSemaphoreGive(peersMutex);
}

bool ESPNowManager::hasPeer(const uint8_t* mac) {
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bool result = findPeerIndex(mac) >= 0;
    xSemaphoreGive(peersMutex);
    return result;
}

ESPNowPeer* ESPNowManager::getPeer(const uint8_t* mac) {
    // ACHTUNG: Nicht thread-safe! Nur aus Main-Thread aufrufen
    int index = findPeerIndex(mac);
    if (index >= 0) {
        return &peers[index];
    }
    return nullptr;
}

bool ESPNowManager::isConnected() {
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bool result = false;
    for (auto& peer : peers) {
        if (peer.connected) {
            result = true;
            break;
        }
    }
    xSemaphoreGive(peersMutex);
    return result;
}

bool ESPNowManager::isPeerConnected(const uint8_t* mac) {
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bool result = false;
    int index = findPeerIndex(mac);
    if (index >= 0) {
        result = peers[index].connected;
    }
    xSemaphoreGive(peersMutex);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// DATEN SENDEN (direkt, ESP-NOW ist bereits async!)
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowManager::send(const uint8_t* mac, const ESPNowPacket& packet) {
    if (!initialized) {
        DEBUG_PRINTLN("ESPNowManager: ❌ Nicht initialisiert!");
        return false;
    }

    if (!packet.isValid()) {
        DEBUG_PRINTLN("ESPNowManager: ❌ Ungültiges Paket!");
        return false;
    }

    // MAC für Broadcast
    uint8_t targetMac[6];
    if (mac) {
        memcpy(targetMac, mac, 6);
    } else {
        memset(targetMac, 0xFF, 6);  // Broadcast
    }

    // DIREKT senden - esp_now_send ist bereits nicht-blockierend!
    esp_err_t result = esp_now_send(targetMac, packet.getRawData(), packet.getTotalLength());
    
    if (result != ESP_OK) {
        DEBUG_PRINTF("ESPNowManager: ⚠️ esp_now_send() fehlgeschlagen: %d\n", result);
        return false;
    }

    // Statistik aktualisieren (mit Mutex)
    if (mac && xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        int index = findPeerIndex(mac);
        if (index >= 0) {
            peers[index].packetsSent++;
        }
        xSemaphoreGive(peersMutex);
    }

    return true;
}

bool ESPNowManager::broadcast(const ESPNowPacket& packet) {
    return send(nullptr, packet);
}

void ESPNowManager::sendHeartbeat() {
    ESPNowPacket hb;
    hb.begin(MainCmd::HEARTBEAT);
    
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (auto& peer : peers) {
            send(peer.mac, hb);
        }
        xSemaphoreGive(peersMutex);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// HEARTBEAT & TIMEOUT
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::setHeartbeat(bool enabled, uint32_t intervalMs) {
    heartbeatEnabled = enabled;
    heartbeatInterval = intervalMs;
    DEBUG_PRINTF("ESPNowManager: Heartbeat %s (%dms)\n", enabled ? "AN" : "AUS", intervalMs);
}

void ESPNowManager::setTimeout(uint32_t timeout) {
    timeoutMs = timeout;
    DEBUG_PRINTF("ESPNowManager: Timeout: %dms\n", timeout);
}

void ESPNowManager::setMaxPeers(uint8_t maxPeers) {
    // User-Limit validieren (1-20, nicht größer als Hardware-Limit)
    if (maxPeers == 0) maxPeers = 1;
    if (maxPeers > ESPNOW_MAX_PEERS_LIMIT) maxPeers = ESPNOW_MAX_PEERS_LIMIT;
    
    maxPeersLimit = maxPeers;
    DEBUG_PRINTF("ESPNowManager: MaxPeers Limit: %d (hardware limit: %d)\n", maxPeersLimit, ESPNOW_MAX_PEERS_LIMIT);
}

void ESPNowManager::checkTimeouts() {
    unsigned long now = millis();

    // Mit Mutex schützen
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    for (auto& peer : peers) {
        if (peer.connected) {
            if (peer.lastSeen > 0 && (now - peer.lastSeen) > timeoutMs) {
                peer.connected = false;
                
                DEBUG_PRINTF("ESPNowManager: ⚠️ Peer %s Timeout!\n", macToString(peer.mac).c_str());

                ESPNowEventData eventData = {};
                eventData.event = ESPNowEvent::PEER_DISCONNECTED;
                memcpy(eventData.mac, peer.mac, 6);
                
                // Events nach Mutex-Release triggern
                xSemaphoreGive(peersMutex);
                triggerEvent(ESPNowEvent::PEER_DISCONNECTED, &eventData);
                triggerEvent(ESPNowEvent::HEARTBEAT_TIMEOUT, &eventData);
                
                // Mutex wieder holen für Loop
                if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
                    return;
                }
            }
        }
    }
    
    xSemaphoreGive(peersMutex);
}

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::setReceiveCallback(ESPNowReceiveCallback callback) {
    receiveCallback = callback;
}

void ESPNowManager::setSendCallback(ESPNowSendCallback callback) {
    sendCallback = callback;
}

void ESPNowManager::onEvent(ESPNowEvent event, ESPNowEventCallback callback) {
    int idx = static_cast<int>(event);
    if (idx >= 0 && idx < 12) {
        eventCallbacks[idx] = callback;
    }
}

void ESPNowManager::offEvent(ESPNowEvent event) {
    int idx = static_cast<int>(event);
    if (idx >= 0 && idx < 12) {
        eventCallbacks[idx] = nullptr;
    }
}

void ESPNowManager::triggerEvent(ESPNowEvent event, ESPNowEventData* data) {
    int idx = static_cast<int>(event);
    if (idx >= 0 && idx < 12 && eventCallbacks[idx]) {
        eventCallbacks[idx](data);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATISCHE ESP-NOW CALLBACKS (minimal - nur Queue!)
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::onDataRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    extern ESPNowManager espNow;
    
    if (!espNow.rxQueue || !info || !data || len <= 0) return;
    
    // Direkt in Queue schieben (im WiFi-ISR-Kontext!)
    RxQueueItem item;
    memcpy(item.mac, info->src_addr, 6);
    memcpy(item.data, data, len);
    item.length = len;
    item.timestamp = millis();
    
    // Non-blocking, von ISR aus
    xQueueSendFromISR(espNow.rxQueue, &item, nullptr);
}

void ESPNowManager::onDataSentStatic(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
    extern ESPNowManager espNow;
    
    // tx_info enthält MAC-Adresse nicht direkt - verwende nullptr
    espNow.handleSendStatus(nullptr, status == ESP_NOW_SEND_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// RX-QUEUE VERARBEITUNG (im Main-Thread via update())
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::processRxQueue() {
    RxQueueItem rxItem;
    
    // Alle verfügbaren RX-Items verarbeiten
    while (xQueueReceive(rxQueue, &rxItem, 0) == pdTRUE) {
        
        // Paket parsen
        ESPNowPacket packet;
        if (!packet.parse(rxItem.data, rxItem.length)) {
            DEBUG_PRINTLN("ESPNowManager: ⚠️ Paket-Parse fehlgeschlagen");
            continue;
        }
        
        // Peer aktualisieren (mit Mutex)
        bool wasDisconnected = false;
        if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            int index = findPeerIndex(rxItem.mac);
            if (index >= 0) {
                wasDisconnected = !peers[index].connected;
                peers[index].connected = true;
                peers[index].lastSeen = rxItem.timestamp;
                peers[index].packetsReceived++;
            }
            xSemaphoreGive(peersMutex);
        }
        
        // Connected-Event triggern (außerhalb Mutex!)
        if (wasDisconnected) {
            DEBUG_PRINTF("ESPNowManager: ✅ Peer %s verbunden\n", macToString(rxItem.mac).c_str());
            
            ESPNowEventData eventData = {};
            eventData.event = ESPNowEvent::PEER_CONNECTED;
            memcpy(eventData.mac, rxItem.mac, 6);
            triggerEvent(ESPNowEvent::PEER_CONNECTED, &eventData);
        }
        
        // Nach MainCmd verarbeiten
        MainCmd cmd = packet.getMainCmd();
        
        if (cmd == MainCmd::HEARTBEAT) {
            // Heartbeat-Event
            ESPNowEventData eventData = {};
            eventData.event = ESPNowEvent::HEARTBEAT_RECEIVED;
            memcpy(eventData.mac, rxItem.mac, 6);
            triggerEvent(ESPNowEvent::HEARTBEAT_RECEIVED, &eventData);
            continue;
        }
        
        // User-Callback
        if (receiveCallback) {
            receiveCallback(rxItem.mac, packet);
        }
        
        // Data-Received Event
        ESPNowEventData eventData = {};
        eventData.event = ESPNowEvent::DATA_RECEIVED;
        memcpy(eventData.mac, rxItem.mac, 6);
        eventData.packet = &packet;
        triggerEvent(ESPNowEvent::DATA_RECEIVED, &eventData);
    }
}

void ESPNowManager::handleSendStatus(const uint8_t* mac, bool success) {
    // Statistik aktualisieren (mit Mutex)
    if (mac && !success && xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        int index = findPeerIndex(mac);
        if (index >= 0) {
            peers[index].packetsLost++;
        }
        xSemaphoreGive(peersMutex);
    }

    // User-Callback
    if (sendCallback) {
        sendCallback(mac, success);
    }

    // Events triggern
    ESPNowEventData eventData = {};
    if (mac) {
        memcpy(eventData.mac, mac, 6);
    }
    eventData.success = success;

    if (success) {
        eventData.event = ESPNowEvent::SEND_SUCCESS;
        triggerEvent(ESPNowEvent::SEND_SUCCESS, &eventData);
    } else {
        eventData.event = ESPNowEvent::SEND_FAILED;
        triggerEvent(ESPNowEvent::SEND_FAILED, &eventData);
    }

    eventData.event = ESPNowEvent::DATA_SENT;
    triggerEvent(ESPNowEvent::DATA_SENT, &eventData);
}

// ═══════════════════════════════════════════════════════════════════════════
// UPDATE (Main-Thread)
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::update() {
    if (!initialized) return;

    unsigned long now = millis();

    // Heartbeat senden
    if (heartbeatEnabled && (now - lastHeartbeatSent) >= heartbeatInterval) {
        sendHeartbeat();
        lastHeartbeatSent = now;
    }

    // Timeouts prüfen
    checkTimeouts();
    
    // RX-Queue verarbeiten
    processRxQueue();
}

int ESPNowManager::getQueuePending() {
    return rxQueue ? uxQueueMessagesWaiting(rxQueue) : 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowManager::getOwnMac(uint8_t* mac) {
    if (mac) {
        WiFi.macAddress(mac);
    }
}

String ESPNowManager::getOwnMacString() {
    uint8_t mac[6];
    getOwnMac(mac);
    return macToString(mac);
}

String ESPNowManager::macToString(const uint8_t* mac) {
    if (!mac) return "NULL";
    
    char buffer[18];
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buffer);
}

bool ESPNowManager::stringToMac(const char* macStr, uint8_t* mac) {
    if (!macStr || !mac) return false;

    int values[6];
    int count = sscanf(macStr, "%x:%x:%x:%x:%x:%x",
                       &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5]);

    if (count != 6) return false;

    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }

    return true;
}

int ESPNowManager::findPeerIndex(const uint8_t* mac) {
    if (!mac) return -1;

    for (int i = 0; i < peers.size(); i++) {
        if (compareMac(peers[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

bool ESPNowManager::compareMac(const uint8_t* mac1, const uint8_t* mac2) {
    if (!mac1 || !mac2) return false;
    return memcmp(mac1, mac2, 6) == 0;
}

void ESPNowManager::printInfo() {
    DEBUG_PRINTLN("\n╔═══════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║          ESP-NOW MANAGER INFO                 ║");
    DEBUG_PRINTLN("╚═══════════════════════════════════════════════╝");
    
    DEBUG_PRINTF("Status:     %s\n", initialized ? "✅ Initialisiert" : "❌ Nicht init");
    DEBUG_PRINTF("MAC:        %s\n", getOwnMacString().c_str());
    DEBUG_PRINTF("Kanal:      %d\n", wifiChannel);
    DEBUG_PRINTF("Heartbeat:  %s (%dms)\n", heartbeatEnabled ? "AN" : "AUS", heartbeatInterval);
    DEBUG_PRINTF("Timeout:    %dms\n", timeoutMs);
    DEBUG_PRINTLN("Protokoll:  [MAIN_CMD] [TOTAL_LEN] [SUB_CMD] [LEN] [DATA]...");
    DEBUG_PRINTLN("Threading:  ❌ KEIN Worker-Thread (ESP-NOW ist async!)");
    
    // Queue-Statistiken
    DEBUG_PRINTLN("\n─── Queue ─────────────────────────────────────");
    DEBUG_PRINTF("RX-Queue:   %d / %d\n", getQueuePending(), ESPNOW_RX_QUEUE_SIZE);
    
    DEBUG_PRINTLN("\n─── Peers ─────────────────────────────────────");
    
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        DEBUG_PRINTF("Anzahl: %d / %d\n", peers.size(), ESPNOW_MAX_PEERS_LIMIT);
        
        for (auto& peer : peers) {
            DEBUG_PRINTF("\n  MAC: %s\n", macToString(peer.mac).c_str());
            DEBUG_PRINTF("  Status:     %s\n", peer.connected ? "✅ Verbunden" : "❌ Getrennt");
            DEBUG_PRINTF("  LastSeen:   %lums ago\n", peer.lastSeen > 0 ? (millis() - peer.lastSeen) : 0);
            DEBUG_PRINTF("  RX/TX/Lost: %lu / %lu / %lu\n", 
                         peer.packetsReceived, peer.packetsSent, peer.packetsLost);
        }
        
        xSemaphoreGive(peersMutex);
    }
    
    DEBUG_PRINTLN("\n═══════════════════════════════════════════════\n");
}