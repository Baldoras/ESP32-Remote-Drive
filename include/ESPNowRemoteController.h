/**
 * ESPNowRemoteController.h
 * 
 * ESP-NOW Remote Controller für Drive System
 * Erweitert ESPNowManager um Pairing mit MAC-Validierung
 */

#ifndef ESP_NOW_REMOTE_CONTROLLER_H
#define ESP_NOW_REMOTE_CONTROLLER_H

#include "ESPNowManager.h"
#include "ESPNowPacket.h"

class ESPNowRemoteController : public ESPNowManager {
public:
    ESPNowRemoteController();
    ~ESPNowRemoteController() override;
    
    /**
     * RX-Verarbeitung mit MAC-Validierung & Pairing
     */
    void processRxQueue() override;

private:
    /**
     * MAC-Validierung
     */
    bool isValidMasterMac(const uint8_t* mac);
    
    /**
     * PAIR_REQUEST verarbeiten
     */
    void handlePairRequest(const uint8_t* mac, unsigned long timestamp);
};

#endif