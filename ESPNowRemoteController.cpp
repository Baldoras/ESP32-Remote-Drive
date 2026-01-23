/**
 * ESPNowRemoteController.cpp
 * 
 * Implementation mit Debug (ohne protected members)
 */

#include "include/ESPNowRemoteController.h"
#include "include/ESPNowPacket.h"
#include "include/UserConfig.h"
#include "include/MotorController.h"
#include "include/Globals.h"

extern UserConfig userConfig;
extern MotorController motorCtrl;

// Joystick-Daten Struktur
struct __attribute__((packed)) JoystickData {
    int16_t x;
    int16_t y;
    uint8_t btn;
};

ESPNowRemoteController::ESPNowRemoteController()
    : ESPNowManager()
{
    Serial.println("[ESPNowRemoteController] Constructor");
    Serial.printf("[ESPNowRemoteController] JoystickData size: %d bytes\n", sizeof(JoystickData));
}

ESPNowRemoteController::~ESPNowRemoteController() {
    Serial.println("[ESPNowRemoteController] Destructor");
}

// ═══════════════════════════════════════════════════════════════════════════
// MAC-VALIDIERUNG
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowRemoteController::isValidMasterMac(const uint8_t* mac) {
    if (!mac) return false;
    
    const char* configMac = userConfig.getEspnowPeerMac();
    uint8_t masterMac[6];
    
    if (!stringToMac(configMac, masterMac)) {
        return false;
    }
    
    return compareMac(mac, masterMac);
}

// ═══════════════════════════════════════════════════════════════════════════
// PAIR_REQUEST HANDLER
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowRemoteController::handlePairRequest(const uint8_t* mac, unsigned long timestamp) {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     PAIR_REQUEST HANDLER               ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    if (!isValidMasterMac(mac)) {
        Serial.println("❌ REJECTED: Invalid MAC!");
        
        ESPNowPacket errorPacket;
        errorPacket.begin(MainCmd::ERROR);
        uint8_t errorCode = 0x01;
        errorPacket.addByte(DataCmd::ERROR_CODE, errorCode);
        send(mac, errorPacket);
        return;
    }
    
    if (!hasPeer(mac)) {
        if (!addPeer(mac, false)) {
            Serial.println("❌ addPeer() FAILED!");
            return;
        }
    }
    
    if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int index = findPeerIndex(mac);
        if (index >= 0) {
            peers[index].connected = true;
            peers[index].lastSeen = timestamp;
        }
        xSemaphoreGive(peersMutex);
    }
    
    ESPNowPacket response;
    response.begin(MainCmd::PAIR_RESPONSE);
    send(mac, response);
    
    ESPNowEventData eventData = {};
    eventData.event = ESPNowEvent::PEER_CONNECTED;
    memcpy(eventData.mac, mac, 6);
    triggerEvent(ESPNowEvent::PEER_CONNECTED, &eventData);
    
    Serial.println("✅ PAIRING SUCCESSFUL!\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// RX-QUEUE VERARBEITUNG
// ═══════════════════════════════════════════════════════════════════════════

void ESPNowRemoteController::processRxQueue() {
    if (!rxQueue) return;
    
    int pending = uxQueueMessagesWaiting(rxQueue);
    if (pending == 0) return;
    
    RxQueueItem rxItem;
    
    while (xQueueReceive(rxQueue, &rxItem, 0) == pdTRUE) {
        
        // Parse
        ESPNowPacket packet;
        if (!packet.parse(rxItem.data, rxItem.length)) {
            Serial.println("[RX] ❌ Parse FAILED!");
            continue;
        }
        
        MainCmd cmd = packet.getMainCmd();
        
        // ═════════════════════════════════════════════════════════════════
        // PAIR_REQUEST
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::PAIR_REQUEST) {
            handlePairRequest(rxItem.mac, rxItem.timestamp);
            continue;
        }
        
        // ═════════════════════════════════════════════════════════════════
        // HEARTBEAT
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::HEARTBEAT) {
            if (xSemaphoreTake(peersMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                int index = findPeerIndex(rxItem.mac);
                if (index >= 0) {
                    peers[index].connected = true;
                    peers[index].lastSeen = rxItem.timestamp;
                    peers[index].packetsReceived++;
                }
                xSemaphoreGive(peersMutex);
            }
            
            ESPNowPacket ackPacket;
            ackPacket.begin(MainCmd::ACK);
            send(rxItem.mac, ackPacket);
            
            continue;
        }
        
        // ═════════════════════════════════════════════════════════════════
        // JOYSTICK DATA - MIT DEBUG
        // ═════════════════════════════════════════════════════════════════
        if (cmd == MainCmd::USER_START || cmd == MainCmd::DATA_REQUEST) {
            
            Serial.println("\n──── JOYSTICK DATA ────");
            Serial.printf("MainCmd: 0x%02X\n", static_cast<uint8_t>(cmd));
            Serial.printf("Entries: %d\n", packet.getEntryCount());
            
            // JOYSTICK_ALL prüfen
            if (packet.has(DataCmd::JOYSTICK_ALL)) {
                Serial.println("✅ Has JOYSTICK_ALL (0x13)");
                
                size_t dataLen = 0;
                const uint8_t* data = packet.getData(DataCmd::JOYSTICK_ALL, &dataLen);
                
                Serial.printf("Data length: %d bytes (expected: %d)\n", dataLen, sizeof(JoystickData));
                
                if (data && dataLen > 0) {
                    // Hex dump der Rohdaten
                    Serial.print("Raw data: ");
                    for (size_t i = 0; i < dataLen; i++) {
                        Serial.printf("%02X ", data[i]);
                    }
                    Serial.println();
                    
                    // Prüfe Länge
                    if (dataLen >= sizeof(JoystickData)) {
                        const JoystickData* joyData = packet.get<JoystickData>(DataCmd::JOYSTICK_ALL);
                        
                        if (joyData) {
                            Serial.printf("✅ Joystick: X=%d, Y=%d, Btn=%d\n", 
                                         joyData->x, joyData->y, joyData->btn);
                            
                            // An Motor weitergeben
                            motorCtrl.processMovementInput((int8_t)joyData->x, (int8_t)joyData->y);
                            
                        } else {
                            Serial.println("❌ get<JoystickData>() returned NULL!");
                        }
                    } else {
                        Serial.printf("❌ Data too short: %d < %d bytes\n", dataLen, sizeof(JoystickData));
                        
                        // Versuche manuelle Extraktion (Little Endian)
                        if (dataLen >= 5) {
                            int16_t x = (int16_t)((data[1] << 8) | data[0]);
                            int16_t y = (int16_t)((data[3] << 8) | data[2]);
                            uint8_t btn = data[4];
                            
                            Serial.printf("⚠️  Manual extract: X=%d, Y=%d, Btn=%d\n", x, y, btn);
                            motorCtrl.processMovementInput((int8_t)x, (int8_t)y);
                        }
                    }
                } else {
                    Serial.println("❌ getData() returned NULL or length=0");
                }
                
            } else {
                Serial.println("❌ JOYSTICK_ALL (0x13) NOT found!");
                
                // Fallback: Einzelne X/Y Werte (0x10, 0x11)
                if (packet.has(DataCmd::JOYSTICK_X) && packet.has(DataCmd::JOYSTICK_Y)) {
                    int16_t joyX, joyY;
                    
                    if (packet.getInt16(DataCmd::JOYSTICK_X, joyX) &&
                        packet.getInt16(DataCmd::JOYSTICK_Y, joyY)) {
                        
                        Serial.printf("✅ Joystick (separate): X=%d, Y=%d\n", joyX, joyY);
                        motorCtrl.processMovementInput((int8_t)joyX, (int8_t)joyY);
                    }
                } else {
                    Serial.println("❌ No joystick data found!");
                }
            }
            
            Serial.println("──────────────────────");
            
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
    }
}