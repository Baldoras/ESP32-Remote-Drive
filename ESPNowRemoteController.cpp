/**
 * ESPNowRemoteController.cpp
 * 
 * Implementation mit vollständigem Pairing-Handling
 */

#include "include/ESPNowRemoteController.h"
#include "include/ESPNowPacket.h"
#include "include/UserConfig.h"
#include "include/Globals.h"

extern UserConfig userConfig;

ESPNowRemoteController::ESPNowRemoteController()
    : ESPNowManager()
{
    Serial.println("[ESPNowRemoteController] Constructor");
}

ESPNowRemoteController::~ESPNowRemoteController() {
    Serial.println("[ESPNowRemoteController] Destructor");
}

// ═══════════════════════════════════════════════════════════════════════════
// MAC-VALIDIERUNG
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowRemoteController::isValidMasterMac(const uint8_t* mac) {
    if (!mac) {
        Serial.println("[isValidMasterMac] ❌ mac is NULL!");
        return false;
    }
    
    const char* configMac = userConfig.getEspnowPeerMac();
    
    uint8_t masterMac[6];
    if (!stringToMac(configMac, masterMac)) {
        Serial.printf("[isValidMasterMac] ❌ Invalid config MAC: %s\n", configMac);
        return false;
    }
    
    bool match = compareMac(mac, masterMac);
    
    Serial.printf("[isValidMasterMac] %s vs %s = %s\n",
                 macToString(mac).c_str(),
                 configMac,
                 match ? "MATCH" : "NO MATCH");
    
    return match;
}

// ═══════════════════════════════════════════════════════════════════════════
// PAIR_REQUEST HANDLER - VOLLSTÄNDIG
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowRemoteController::handlePairRequest(const uint8_t* mac, unsigned long timestamp) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     PAIR_REQUEST HANDLER               ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.printf("From: %s\n", macToString(mac).c_str());
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 1: MAC-Validierung
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[1/6] MAC-Validierung...");
    
    if (!isValidMasterMac(mac)) {
        Serial.println("  ❌ REJECTED: Invalid Master MAC!");
        
        ESPNowPacket errorPacket;
        errorPacket.begin(MainCmd::ERROR);
        uint8_t errorCode = 0x01;  // Invalid MAC
        errorPacket.addByte(DataCmd::ERROR_CODE, errorCode);
        
        Serial.println("  → Sending ERROR 0x01");
        send(mac, errorPacket);
        
        Serial.println("════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("  ✅ MAC validated - Master accepted!");
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 2: Peer hinzufügen (falls noch nicht vorhanden)
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[2/6] Adding peer...");
    
    bool wasAlreadyPeer = hasPeer(mac);
    
    if (!wasAlreadyPeer) {
        Serial.println("  → Calling addPeer()...");
        
        if (!addPeer(mac, false)) {
            Serial.println("  ❌ addPeer() FAILED!");
            Serial.println("  Possible reasons:");
            Serial.println("    - Peer limit reached");
            Serial.println("    - ESP-NOW error");
            Serial.println("════════════════════════════════════════\n");
            return;
        }
        
        Serial.println("  ✅ Peer added successfully");
    } else {
        Serial.println("  ℹ️  Peer already exists (re-pairing)");
    }
    
    // Verify
    if (!hasPeer(mac)) {
        Serial.println("  ❌ FATAL ERROR: hasPeer() returns false after addPeer()!");
        Serial.println("════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("  ✅ Peer confirmed in peer list");
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 3: Peer-Status auf CONNECTED setzen
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[3/6] Setting peer status to CONNECTED...");
    
    bool statusUpdated = false;
    
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int index = findPeerIndex(mac);
        Serial.printf("  Peer index: %d\n", index);
        
        if (index >= 0) {
            // Status auf CONNECTED setzen
            bool wasConnected = peers[index].connected;
            peers[index].connected = true;
            peers[index].lastSeen = timestamp;
            
            Serial.printf("  Previous status: %s\n", wasConnected ? "CONNECTED" : "DISCONNECTED");
            Serial.println("  ✅ Peer marked as CONNECTED");
            Serial.printf("  Last seen: %lu ms\n", timestamp);
            
            statusUpdated = true;
        } else {
            Serial.println("  ❌ Peer index not found!");
        }
        
        xSemaphoreGive(peersMutex);
    } else {
        Serial.println("  ❌ Mutex timeout!");
    }
    
    if (!statusUpdated) {
        Serial.println("  ⚠️  Status update failed - continuing anyway");
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 4: PAIR_RESPONSE Paket erstellen
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[4/6] Creating PAIR_RESPONSE packet...");
    
    ESPNowPacket response;
    response.begin(MainCmd::PAIR_RESPONSE);
    
    Serial.printf("  MainCmd: 0x%02X (PAIR_RESPONSE)\n", static_cast<uint8_t>(MainCmd::PAIR_RESPONSE));
    Serial.printf("  Packet length: %d bytes\n", response.getTotalLength());
    Serial.printf("  Target MAC: %s\n", macToString(mac).c_str());
    
    // Optional: Eigene MAC als Bestätigung mitschicken
    // uint8_t ownMac[6];
    // getOwnMac(ownMac);
    // response.add(DataCmd::RAW_DATA_1, ownMac, 6);
    
    Serial.println("  ✅ PAIR_RESPONSE packet ready");
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 5: PAIR_RESPONSE senden
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[5/6] Sending PAIR_RESPONSE...");
    Serial.printf("  Current peer count: %d\n", getPeerCount());
    Serial.printf("  Has peer (double-check): %s\n", hasPeer(mac) ? "YES" : "NO");
    
    bool sendResult = send(mac, response);
    
    if (!sendResult) {
        Serial.println("  ❌ SEND FAILED!");
        Serial.println("  Possible reasons:");
        Serial.println("    - Peer not properly registered");
        Serial.println("    - ESP-NOW send error");
        Serial.println("    - Buffer full");
        Serial.println("════════════════════════════════════════\n");
        return;
    }
    
    Serial.println("  ✅ PAIR_RESPONSE sent successfully!");
    
    // ─────────────────────────────────────────────────────────────────────
    // SCHRITT 6: PEER_CONNECTED Event triggern
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n[6/6] Triggering PEER_CONNECTED event...");
    
    ESPNowEventData eventData = {};
    eventData.event = ESPNowEvent::PEER_CONNECTED;
    memcpy(eventData.mac, mac, 6);
    triggerEvent(ESPNowEvent::PEER_CONNECTED, &eventData);
    
    Serial.println("  ✅ Event triggered");
    
    // ─────────────────────────────────────────────────────────────────────
    // PAIRING ABGESCHLOSSEN
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("\n  ╔════════════════════════════════════╗");
    Serial.println("  ║   🎉 PAIRING SUCCESSFUL! 🎉        ║");
    Serial.println("  ╚════════════════════════════════════╝");
    Serial.printf("  Master: %s\n", macToString(mac).c_str());
    Serial.println("  Status: CONNECTED");
    Serial.println("  Ready to receive commands!");
    Serial.println("════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// RX-QUEUE VERARBEITUNG
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowRemoteController::processRxQueue() {
    if (!rxQueue) {
        Serial.println("[processRxQueue] ❌ rxQueue is NULL!");
        return;
    }
    
    int pending = uxQueueMessagesWaiting(rxQueue);
    if (pending == 0) {
        return; // Nichts zu tun
    }
    
    Serial.printf("\n[processRxQueue] Processing %d packets...\n", pending);
    
    RxQueueItem rxItem;
    int processed = 0;
    
    while (xQueueReceive(rxQueue, &rxItem, 0) == pdTRUE) {
        processed++;
        
        Serial.println("\n────────────────────────────────────────");
        Serial.printf("Packet #%d\n", processed);
        Serial.printf("  From: %s\n", macToString(rxItem.mac).c_str());
        Serial.printf("  Length: %d bytes\n", rxItem.length);
        
        // Hex dump (erste 20 Bytes)
        Serial.print("  Data: ");
        for (int i = 0; i < min((int)rxItem.length, 20); i++) {
            Serial.printf("%02X ", rxItem.data[i]);
        }
        if (rxItem.length > 20) Serial.print("...");
        Serial.println();
        
        // Parse
        ESPNowPacket packet;
        if (!packet.parse(rxItem.data, rxItem.length)) {
            Serial.println("  ❌ Parse FAILED!");
            continue;
        }
        
        Serial.println("  ✅ Parse OK");
        
        MainCmd cmd = packet.getMainCmd();
        Serial.printf("  MainCmd: 0x%02X\n", static_cast<uint8_t>(cmd));
        
        // ═════════════════════════════════════════════════════════════════
        // PAIR_REQUEST - Vollständiges Pairing-Handling
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::PAIR_REQUEST) {
            Serial.println("  → PAIR_REQUEST detected");
            handlePairRequest(rxItem.mac, rxItem.timestamp);
            continue;
        }
        
        // ═════════════════════════════════════════════════════════════════
        // HEARTBEAT
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::HEARTBEAT) {
            Serial.println("  → HEARTBEAT received");
            
            // Peer aktualisieren
            if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                int index = findPeerIndex(rxItem.mac);
                if (index >= 0) {
                    peers[index].connected = true;
                    peers[index].lastSeen = rxItem.timestamp;
                    peers[index].packetsReceived++;
                }
                xSemaphoreGive(peersMutex);
            }
            
            // Event
            ESPNowEventData eventData = {};
            eventData.event = ESPNowEvent::HEARTBEAT_RECEIVED;
            memcpy(eventData.mac, rxItem.mac, 6);
            triggerEvent(ESPNowEvent::HEARTBEAT_RECEIVED, &eventData);
            
            continue;
        }
        
        // ═════════════════════════════════════════════════════════════════
        // JOYSTICK DATA
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::USER_START || cmd == MainCmd::DATA_REQUEST) {
            
            if (packet.has(DataCmd::JOYSTICK_X) && packet.has(DataCmd::JOYSTICK_Y)) {
                int16_t joyX, joyY;
                
                if (packet.getInt16(DataCmd::JOYSTICK_X, joyX) &&
                    packet.getInt16(DataCmd::JOYSTICK_Y, joyY)) {
                    
                    Serial.printf("  → Joystick: X=%d, Y=%d\n", joyX, joyY);
                    
                    // Callback aufrufen (wird in main .ino gesetzt)
                    if (receiveCallback) {
                        receiveCallback(rxItem.mac, packet);
                    }
                    
                    // Event
                    ESPNowEventData eventData = {};
                    eventData.event = ESPNowEvent::DATA_RECEIVED;
                    memcpy(eventData.mac, rxItem.mac, 6);
                    eventData.packet = &packet;
                    triggerEvent(ESPNowEvent::DATA_RECEIVED, &eventData);
                }
            }
            
            // Peer aktualisieren
            if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                int index = findPeerIndex(rxItem.mac);
                if (index >= 0) {
                    peers[index].connected = true;
                    peers[index].lastSeen = rxItem.timestamp;
                    peers[index].packetsReceived++;
                }
                xSemaphoreGive(peersMutex);
            }
            
            continue;
        }
        
        // ═════════════════════════════════════════════════════════════════
        // ANDERE COMMANDS
        // ═════════════════════════════════════════════════════════════════
        Serial.printf("  → Unhandled MainCmd: 0x%02X\n", static_cast<uint8_t>(cmd));
    }
    
    Serial.println("────────────────────────────────────────");
    Serial.printf("[processRxQueue] Processed %d packets\n\n", processed);
}