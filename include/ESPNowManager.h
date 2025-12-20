/**
 * ESPNowManager.h
 * 
 * Universelle ESP-NOW Kommunikationsklasse mit TLV-Protokoll
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
 */

#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <functional>
#include <vector>
#include "setupConf.h"

// Internes Hardware-Limit für Peers (ESP-NOW Hardware-Beschränkung)
#ifndef ESPNOW_MAX_PEERS_LIMIT
#define ESPNOW_MAX_PEERS_LIMIT  20      // ESP-NOW Hardware-Maximum
#endif

// ═══════════════════════════════════════════════════════════════════════════
// KONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

// Queue-Größen, Worker-Task Parameter und Packet-Größen sind in setupConf.h definiert

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND ENUMS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Haupt-Commands (Main-CMD)
 */
enum class MainCmd : uint8_t {
    NONE            = 0x00,
    HEARTBEAT       = 0x01,     // Heartbeat/Ping
    ACK             = 0x02,     // Bestätigung
    DATA_REQUEST    = 0x03,     // Datenanfrage
    DATA_RESPONSE   = 0x04,     // Datenantwort
    PAIR_REQUEST    = 0x05,     // Pairing-Anfrage
    PAIR_RESPONSE   = 0x06,     // Pairing-Antwort
    ERROR           = 0x07,     // Fehlermeldung
    
    // User-Commands ab 0x10
    USER_START      = 0x10
};

/**
 * Sub-Commands / Data-Identifier
 */
enum class DataCmd : uint8_t {
    NONE            = 0x00,
    
    // Joystick (0x01-0x0F)
    JOYSTICK_X      = 0x01,     // int16_t
    JOYSTICK_Y      = 0x02,     // int16_t
    JOYSTICK_BTN    = 0x03,     // uint8_t (0/1)
    JOYSTICK_ALL    = 0x04,     // struct { int16_t x, y; uint8_t btn; }
    
    // Buttons/Inputs (0x10-0x1F)
    BUTTON_STATE    = 0x10,     // uint8_t (Bitmask)
    SWITCH_STATE    = 0x11,     // uint8_t (Bitmask)
    POTENTIOMETER   = 0x12,     // uint16_t (0-4095)
    
    // Motor/Antrieb (0x20-0x2F)
    MOTOR_LEFT      = 0x20,     // int16_t (-100 bis +100)
    MOTOR_RIGHT     = 0x21,     // int16_t (-100 bis +100)
    MOTOR_ALL       = 0x22,     // struct { int16_t left, right; }
    SPEED           = 0x23,     // uint8_t (0-100%)
    
    // Telemetrie (0x30-0x3F)
    BATTERY_VOLTAGE = 0x30,     // uint16_t (mV)
    BATTERY_PERCENT = 0x31,     // uint8_t (0-100%)
    TEMPERATURE     = 0x32,     // int16_t (°C * 10)
    RSSI            = 0x33,     // int8_t (dBm)
    
    // Status (0x40-0x4F)
    CONNECTION      = 0x40,     // uint8_t (0=disconnected, 1=connected)
    ERROR_CODE      = 0x41,     // uint8_t
    MODE            = 0x42,     // uint8_t
    
    // Sensoren (0x50-0x5F)
    DISTANCE        = 0x50,     // uint16_t (mm)
    ACCELERATION    = 0x51,     // struct { int16_t x, y, z; }
    GYROSCOPE       = 0x52,     // struct { int16_t x, y, z; }
    
    // Custom (0xA0-0xFF)
    CUSTOM_1        = 0xA0,
    CUSTOM_2        = 0xA1,
    CUSTOM_3        = 0xA2,
    RAW_DATA        = 0xFF      // Beliebige Rohdaten
};

// ═══════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

class ESPNowManager;
class ESPNowPacket;

// ═══════════════════════════════════════════════════════════════════════════
// QUEUE STRUKTUREN (für Thread-Kommunikation)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Empfangenes Paket für Queue (WiFi-Callback → Worker)
 */
struct RxQueueItem {
    uint8_t mac[6];
    uint8_t data[ESPNOW_MAX_PACKET_SIZE];
    size_t length;
    unsigned long timestamp;
};

/**
 * Zu sendendes Paket für Queue (Main → Worker → WiFi)
 */
struct TxQueueItem {
    uint8_t mac[6];
    uint8_t data[ESPNOW_MAX_PACKET_SIZE];
    size_t length;
    bool broadcast;
};

/**
 * Verarbeitetes Ergebnis für Main-Thread (Worker → Main)
 */
struct ResultQueueItem {
    uint8_t mac[6];                         // Absender-MAC
    MainCmd mainCmd;                        // Haupt-Command
    
    // Geparste Daten (direkt verwendbar)
    struct {
        int16_t joystickX;
        int16_t joystickY;
        uint8_t joystickBtn;
        bool hasJoystick;
        
        int16_t motorLeft;
        int16_t motorRight;
        bool hasMotor;
        
        uint16_t batteryVoltage;
        uint8_t batteryPercent;
        bool hasBattery;
        
        uint8_t buttonState;
        bool hasButtons;
        
        // Raw-Daten für custom Commands
        uint8_t rawData[64];
        size_t rawDataLen;
    } data;
    
    unsigned long timestamp;
};

// ═══════════════════════════════════════════════════════════════════════════
// PAKET-KLASSE MIT BUILDER & PARSER
// ═══════════════════════════════════════════════════════════════════════════

/**
 * ESP-NOW Paket mit Builder-Pattern und Parser
 * 
 * Protokoll: [MAIN_CMD] [TOTAL_LEN] [SUB_CMD] [LEN] [DATA] [SUB_CMD] [LEN] [DATA] ...
 */
class ESPNowPacket {
public:
    ESPNowPacket();
    ~ESPNowPacket();
    
    // ═══════════════════════════════════════════════════════════════════════
    // BUILDER-PATTERN
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Paket starten mit Main-Command
     * @param cmd Haupt-Command
     * @return Referenz für Method-Chaining
     */
    ESPNowPacket& begin(MainCmd cmd);
    
    /**
     * Sub-Daten hinzufügen
     * @param dataCmd Daten-Identifier
     * @param data Daten-Pointer
     * @param len Datenlänge
     * @return Referenz für Method-Chaining
     */
    ESPNowPacket& add(DataCmd dataCmd, const void* data, size_t len);
    
    /**
     * Einzelnes Byte hinzufügen
     */
    ESPNowPacket& addByte(DataCmd dataCmd, uint8_t value);
    
    /**
     * int8_t hinzufügen
     */
    ESPNowPacket& addInt8(DataCmd dataCmd, int8_t value);
    
    /**
     * uint16_t hinzufügen
     */
    ESPNowPacket& addUInt16(DataCmd dataCmd, uint16_t value);
    
    /**
     * int16_t hinzufügen
     */
    ESPNowPacket& addInt16(DataCmd dataCmd, int16_t value);
    
    /**
     * uint32_t hinzufügen
     */
    ESPNowPacket& addUInt32(DataCmd dataCmd, uint32_t value);
    
    /**
     * int32_t hinzufügen
     */
    ESPNowPacket& addInt32(DataCmd dataCmd, int32_t value);
    
    /**
     * float hinzufügen
     */
    ESPNowPacket& addFloat(DataCmd dataCmd, float value);
    
    /**
     * Struct hinzufügen (Template)
     */
    template<typename T>
    ESPNowPacket& addStruct(DataCmd dataCmd, const T& data) {
        return add(dataCmd, &data, sizeof(T));
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // PARSER
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Paket aus Rohdaten parsen
     * @param rawData Empfangene Rohdaten
     * @param len Datenlänge
     * @return true bei Erfolg
     */
    bool parse(const uint8_t* rawData, size_t len);
    
    /**
     * Prüfen ob DataCmd vorhanden ist
     */
    bool has(DataCmd dataCmd) const;
    
    /**
     * Daten-Pointer abrufen (nullptr wenn nicht vorhanden)
     */
    const uint8_t* getData(DataCmd dataCmd, size_t* outLen = nullptr) const;
    
    /**
     * Daten als Template-Typ abrufen (nullptr wenn nicht vorhanden oder falsche Größe)
     */
    template<typename T>
    const T* get(DataCmd dataCmd) const {
        size_t len;
        const uint8_t* data = getData(dataCmd, &len);
        if (data && len >= sizeof(T)) {
            return reinterpret_cast<const T*>(data);
        }
        return nullptr;
    }
    
    /**
     * Byte abrufen
     */
    bool getByte(DataCmd dataCmd, uint8_t& outValue) const;
    
    /**
     * int8_t abrufen
     */
    bool getInt8(DataCmd dataCmd, int8_t& outValue) const;
    
    /**
     * uint16_t abrufen
     */
    bool getUInt16(DataCmd dataCmd, uint16_t& outValue) const;
    
    /**
     * int16_t abrufen
     */
    bool getInt16(DataCmd dataCmd, int16_t& outValue) const;
    
    /**
     * uint32_t abrufen
     */
    bool getUInt32(DataCmd dataCmd, uint32_t& outValue) const;
    
    /**
     * int32_t abrufen
     */
    bool getInt32(DataCmd dataCmd, int32_t& outValue) const;
    
    /**
     * float abrufen
     */
    bool getFloat(DataCmd dataCmd, float& outValue) const;
    
    // ═══════════════════════════════════════════════════════════════════════
    // GETTER
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Main-Command abrufen
     */
    MainCmd getMainCmd() const { return mainCmd; }
    
    /**
     * Rohdaten-Pointer abrufen
     */
    const uint8_t* getRawData() const { return buffer; }
    
    /**
     * Gesamtlänge abrufen (inkl. Header)
     */
    size_t getTotalLength() const { return 2 + dataLength; }
    
    /**
     * Datenlänge abrufen (ohne Header)
     */
    size_t getDataLength() const { return dataLength; }
    
    /**
     * Anzahl der Sub-Einträge
     */
    int getEntryCount() const { return entryCount; }
    
    /**
     * Ist Paket gültig?
     */
    bool isValid() const { return valid; }
    
    /**
     * Paket zurücksetzen
     */
    void clear();
    
    /**
     * Debug-Ausgabe
     */
    void print() const;

private:
    // Paket-Buffer
    uint8_t buffer[ESPNOW_MAX_PACKET_SIZE];
    
    // Parsed Data Index (für schnellen Zugriff)
    struct DataEntry {
        DataCmd cmd;
        uint8_t offset;     // Offset im Buffer (nach Header)
        uint8_t length;
    };
    static const int MAX_ENTRIES = 20;
    DataEntry entries[MAX_ENTRIES];
    int entryCount;
    
    // Status
    MainCmd mainCmd;
    size_t dataLength;      // Länge der Nutzdaten (ohne 2-Byte Header)
    size_t writePos;        // Aktuelle Schreibposition
    bool valid;
    
    // Helper
    int findEntry(DataCmd cmd) const;
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
// HAUPTKLASSE
// ═══════════════════════════════════════════════════════════════════════════

class ESPNowManager {
public:
    
    /**
     * ESP-NOW initialisieren
     * @param channel WiFi-Kanal (0 = auto)
     * @return true bei Erfolg
     */
    bool begin(uint8_t channel = 0);

    /**
     * Konstruktor & Destruktor (public für globale Instanz)
     */
    ESPNowManager();
    ~ESPNowManager();

    /**
     * ESP-NOW beenden und aufräumen
     */
    void end();

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
    // DATEN SENDEN (Thread-safe, via Queue)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Paket an Peer senden (async via Queue)
     * @param mac Ziel-MAC (nullptr = Broadcast)
     * @param packet Zu sendendes Paket
     * @return true wenn in Queue eingereiht
     */
    bool send(const uint8_t* mac, const ESPNowPacket& packet);

    /**
     * Paket an alle Peers senden
     * @return true wenn in Queue eingereiht
     */
    bool broadcast(const ESPNowPacket& packet);

    /**
     * Heartbeat manuell senden
     */
    void sendHeartbeat();

    // ═══════════════════════════════════════════════════════════════════════
    // DATEN EMPFANGEN (Thread-safe, via Queue)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Prüfen ob verarbeitete Daten verfügbar sind
     * @return true wenn Daten in Result-Queue
     */
    bool hasData();

    /**
     * Verarbeitete Daten abrufen (non-blocking)
     * @param result Pointer auf Result-Struktur
     * @return true wenn Daten abgerufen
     */
    bool getData(ResultQueueItem* result);

    /**
     * Alle verfügbaren Daten abrufen und Callback aufrufen
     * @param callback Funktion für jedes Result
     * @return Anzahl verarbeiteter Items
     */
    int processAllData(std::function<void(const ResultQueueItem&)> callback);

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
    // CALLBACKS (Optional, zusätzlich zu Queue)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Empfangs-Callback setzen (wird im Worker-Thread aufgerufen!)
     */
    void setReceiveCallback(ESPNowReceiveCallback callback);

    /**
     * Sende-Callback setzen
     */
    void setSendCallback(ESPNowSendCallback callback);

    /**
     * Event-Callback setzen (UI-Integration, im Main-Thread via update())
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
     * - Verarbeitet Result-Queue
     * - Triggert Events im Main-Thread
     * - Prüft Heartbeat/Timeouts
     */
    void update();

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
    void printInfo();

    /**
     * Queue-Statistiken abrufen
     */
    void getQueueStats(int* rxPending, int* txPending, int* resultPending);

private:
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

    // FreeRTOS Queues
    QueueHandle_t rxQueue;          // WiFi-Callback → Worker
    QueueHandle_t txQueue;          // Main → Worker → WiFi
    QueueHandle_t resultQueue;      // Worker → Main

    // Worker Task
    TaskHandle_t workerTaskHandle;
    volatile bool workerRunning;

    // Callbacks
    ESPNowReceiveCallback receiveCallback;
    ESPNowSendCallback sendCallback;
    ESPNowEventCallback eventCallbacks[12];

    // Statische Callbacks für ESP-NOW
    static void onDataRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    static void onDataSentStatic(const wifi_tx_info_t* tx_info, esp_now_send_status_t status);

    // Worker Task
    static void workerTask(void* parameter);
    void processRxQueue();
    void processTxQueue();

    // Interne Methoden
    void handleSendStatus(const uint8_t* mac, bool success);
    void checkTimeouts();
    void triggerEvent(ESPNowEvent event, ESPNowEventData* data);
    int findPeerIndex(const uint8_t* mac);
    bool compareMac(const uint8_t* mac1, const uint8_t* mac2);
    
    // Paket zu Result konvertieren (im Worker-Thread)
    void packetToResult(const uint8_t* mac, ESPNowPacket& packet, ResultQueueItem& result);
};

#endif // ESP_NOW_MANAGER_H