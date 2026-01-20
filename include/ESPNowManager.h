/**
 * ESPNowManager.h
 * 
 * Generische ESP-NOW Kommunikations-Basisklasse mit TLV-Protokoll
 * OHNE Worker-Thread - direkte Verarbeitung im Main-Thread
 * 
 * Protokoll-Format:
 * [MAIN_CMD 1B] [TOTAL_LEN 1B] [SUB_CMD 1B] [LEN 1B] [DATA...] [SUB_CMD] [LEN] [DATA] ...
 * 
 * Features:
 * - TLV-basiertes Protokoll (Type-Length-Value)
 * - Builder-Pattern für Paket-Erstellung
 * - Parser für einfachen Datenzugriff
 * - Bidirektionale Kommunikation
 * - Heartbeat mit Timeout-Erkennung
 * - Callbacks + UI-Event-Integration
 * - KEINE Worker-Threads (ESP-NOW ist bereits async!)
 * - Erweiterbar durch Vererbung für projekt-spezifische Funktionalität
 */

#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <functional>
#include <vector>
#include "setupConf.h"
#include "ESPNowPacket.h"

// Internes Hardware-Limit für Peers (ESP-NOW Hardware-Beschränkung)
#ifndef ESPNOW_MAX_PEERS_LIMIT
#define ESPNOW_MAX_PEERS_LIMIT  20      // ESP-NOW Hardware-Maximum
#endif

// ═══════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

class ESPNowManager;
class ESPNowPacket;

// ═══════════════════════════════════════════════════════════════════════════
// QUEUE STRUKTUREN
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Empfangenes Paket für Queue (WiFi-ISR → Main-Thread)
 */
struct RxQueueItem {
    uint8_t mac[6];
    uint8_t data[ESPNOW_MAX_PACKET_SIZE];
    size_t length;
    unsigned long timestamp;
};

// ═══════════════════════════════════════════════════════════════════════════
// PEER-STRUKTUR
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Peer-Information
 */
struct ESPNowPeer {
    uint8_t mac[6];             // MAC-Adresse
    bool connected;             // Verbindungsstatus
    unsigned long lastSeen;     // Letzter Empfang (millis)
    uint32_t packetsReceived;   // Empfangene Pakete
    uint32_t packetsSent;       // Gesendete Pakete
    uint32_t packetsLost;       // Verlorene Pakete
    int8_t rssi;                // Signalstärke (falls verfügbar)
};

// ═══════════════════════════════════════════════════════════════════════════
// EVENT-SYSTEM
// ═══════════════════════════════════════════════════════════════════════════

/**
 * ESP-NOW Event-Typen
 */
enum class ESPNowEvent : uint8_t {
    NONE = 0,
    DATA_RECEIVED,      // Daten empfangen
    DATA_SENT,          // Daten gesendet (mit Status)
    PEER_CONNECTED,     // Peer verbunden
    PEER_DISCONNECTED,  // Peer getrennt (Timeout)
    PEER_ADDED,         // Neuer Peer hinzugefügt
    PEER_REMOVED,       // Peer entfernt
    SEND_SUCCESS,       // Senden erfolgreich
    SEND_FAILED,        // Senden fehlgeschlagen
    HEARTBEAT_RECEIVED, // Heartbeat empfangen
    HEARTBEAT_TIMEOUT   // Heartbeat-Timeout
};

/**
 * Event-Daten Struktur
 */
struct ESPNowEventData {
    ESPNowEvent event;          // Event-Typ
    uint8_t mac[6];             // MAC des Peers
    ESPNowPacket* packet;       // Parsed Packet (nur bei DATA_RECEIVED)
    bool success;               // Erfolg (bei SEND)
};

// Callback-Typen
typedef std::function<void(const uint8_t* mac, ESPNowPacket& packet)> ESPNowReceiveCallback;
typedef std::function<void(const uint8_t* mac, bool success)> ESPNowSendCallback;
typedef std::function<void(ESPNowEventData* eventData)> ESPNowEventCallback;

// ═══════════════════════════════════════════════════════════════════════════
// HAUPTKLASSE (Basis)
// ═══════════════════════════════════════════════════════════════════════════

class ESPNowManager {
public:
    
    /**
     * Konstruktor & Destruktor
     */
    ESPNowManager();
    virtual ~ESPNowManager();
    
    /**
     * ESP-NOW initialisieren
     * @param channel WiFi-Kanal (0 = auto)
     * @return true bei Erfolg
     */
    virtual bool begin(uint8_t channel = 0);

    /**
     * ESP-NOW beenden und aufräumen
     */
    virtual void end();

    /**
     * Ist ESP-NOW initialisiert?
     */
    bool isInitialized() const { return initialized; }

    // ═══════════════════════════════════════════════════════════════════════
    // PEER-VERWALTUNG
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Peer hinzufügen
     * @param mac MAC-Adresse (6 Bytes)
     * @param encrypt Verschlüsselung aktivieren
     * @return true bei Erfolg
     */
    bool addPeer(const uint8_t* mac, bool encrypt = false);

    /**
     * Peer entfernen
     */
    bool removePeer(const uint8_t* mac);

    /**
     * Alle Peers entfernen
     */
    void removeAllPeers();

    /**
     * Peer existiert?
     */
    bool hasPeer(const uint8_t* mac);

    /**
     * Peer-Info abrufen
     */
    ESPNowPeer* getPeer(const uint8_t* mac);

    /**
     * Anzahl registrierter Peers
     */
    int getPeerCount() const { return peers.size(); }

    /**
     * Ist mindestens ein Peer verbunden?
     */
    bool isConnected();

    /**
     * Ist spezifischer Peer verbunden?
     */
    bool isPeerConnected(const uint8_t* mac);

    // ═══════════════════════════════════════════════════════════════════════
    // DATEN SENDEN (direkt, esp_now_send ist bereits async!)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Paket an Peer senden
     * @param mac Ziel-MAC (nullptr = Broadcast)
     * @param packet Zu sendendes Paket
     * @return true bei Erfolg
     */
    bool send(const uint8_t* mac, const ESPNowPacket& packet);

    /**
     * Paket an alle Peers senden
     * @return true bei Erfolg
     */
    bool broadcast(const ESPNowPacket& packet);

    /**
     * Heartbeat manuell senden
     */
    void sendHeartbeat();

    // ═══════════════════════════════════════════════════════════════════════
    // HEARTBEAT
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Heartbeat aktivieren/deaktivieren
     */
    void setHeartbeat(bool enabled, uint32_t intervalMs);

    /**
     * Timeout für Verbindungsverlust setzen
     */
    void setTimeout(uint32_t timeoutMs);

    /**
     * Maximale Anzahl Peers setzen (begrenzt Anzahl)
     */
    void setMaxPeers(uint8_t maxPeers);

    // ═══════════════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Empfangs-Callback setzen
     */
    void setReceiveCallback(ESPNowReceiveCallback callback);

    /**
     * Sende-Callback setzen
     */
    void setSendCallback(ESPNowSendCallback callback);

    /**
     * Event-Callback setzen (UI-Integration)
     */
    void onEvent(ESPNowEvent event, ESPNowEventCallback callback);

    /**
     * Event-Callback entfernen
     */
    void offEvent(ESPNowEvent event);

    // ═══════════════════════════════════════════════════════════════════════
    // UPDATE & STATUS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Update-Schleife (in loop() aufrufen!)
     * - Verarbeitet RX-Queue
     * - Triggert Events im Main-Thread
     * - Prüft Heartbeat/Timeouts
     */
    virtual void update();

    /**
     * Eigene MAC-Adresse abrufen
     */
    void getOwnMac(uint8_t* mac);

    /**
     * MAC-Adresse als String
     */
    String getOwnMacString();

    /**
     * MAC zu String konvertieren
     */
    static String macToString(const uint8_t* mac);

    /**
     * String zu MAC konvertieren
     */
    static bool stringToMac(const char* macStr, uint8_t* mac);

    /**
     * Debug-Info ausgeben
     */
    virtual void printInfo();

    /**
     * Queue-Statistiken abrufen
     */
    int getQueuePending();

protected:
    // Statischer Pointer auf die aktive Instanz (für Callbacks)
    static ESPNowManager* instance;
    
    // Status
    bool initialized;
    uint8_t wifiChannel;
    uint8_t maxPeersLimit;       // User-konfigurierbares Peer-Limit (1-20)

    // Peers (mit Mutex geschützt)
    std::vector<ESPNowPeer> peers;
    SemaphoreHandle_t peersMutex;

    // Heartbeat
    bool heartbeatEnabled;
    uint32_t heartbeatInterval;
    uint32_t timeoutMs;
    unsigned long lastHeartbeatSent;

    // FreeRTOS Queue (nur RX)
    QueueHandle_t rxQueue;          // WiFi-ISR → Main-Thread

    // Callbacks
    ESPNowReceiveCallback receiveCallback;
    ESPNowSendCallback sendCallback;
    ESPNowEventCallback eventCallbacks[12];

    // Statische Callbacks für ESP-NOW
    static void onDataRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onDataSentStatic(const wifi_tx_info_t* tx_info, esp_now_send_status_t status);

    // Interne Methoden (protected für Vererbung)
    virtual void processRxQueue();
    virtual void handleSendStatus(const uint8_t* mac, bool success);
    virtual void checkTimeouts();
    void triggerEvent(ESPNowEvent event, ESPNowEventData* data);
    int findPeerIndex(const uint8_t* mac);
    bool compareMac(const uint8_t* mac1, const uint8_t* mac2);
};

#endif // ESP_NOW_MANAGER_H