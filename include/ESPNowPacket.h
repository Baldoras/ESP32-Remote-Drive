/**
 * ESPNowPacket.h
 * 
 * ESP-NOW Paket-Klasse mit TLV-Protokoll
 * Erweitert die generischen Enums aus ESPNowManager um projekt-spezifische Commands
 */

#ifndef ESP_NOW_PACKET_H
#define ESP_NOW_PACKET_H

#include <Arduino.h>
#include "setupConf.h"

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND ENUMS - VOLLSTÄNDIG (Generisch + Projekt-spezifisch)
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
 * Vollständige Liste: Generisch + Remote Control spezifisch
 */
enum class DataCmd : uint8_t {
    NONE            = 0x00,
    
    // Generische Commands (0x01-0x0F)
    TIMESTAMP       = 0x01,     // uint32_t
    SEQUENCE_NUM    = 0x02,     // uint16_t
    STATUS          = 0x03,     // uint8_t
    ERROR_CODE      = 0x04,     // uint8_t
    
    // Joystick (0x10-0x1F)
    JOYSTICK_X      = 0x10,     // int16_t
    JOYSTICK_Y      = 0x11,     // int16_t
    JOYSTICK_BTN    = 0x12,     // uint8_t (0/1)
    JOYSTICK_ALL    = 0x13,     // struct JoystickData
    
    // Buttons/Inputs (0x20-0x2F)
    BUTTON_STATE    = 0x20,     // uint8_t (Bitmask)
    SWITCH_STATE    = 0x21,     // uint8_t (Bitmask)
    POTENTIOMETER   = 0x22,     // uint16_t (0-4095)
    
    // Motor/Antrieb (0x30-0x3F)
    MOTOR_LEFT      = 0x30,     // int16_t (-100 bis +100)
    MOTOR_RIGHT     = 0x31,     // int16_t (-100 bis +100)
    MOTOR_ALL       = 0x32,     // struct MotorData
    SPEED           = 0x33,     // uint8_t (0-100%)
    
    // Telemetrie (0x40-0x4F)
    BATTERY_VOLTAGE = 0x40,     // uint16_t (mV)
    BATTERY_PERCENT = 0x41,     // uint8_t (0-100%)
    TEMPERATURE     = 0x42,     // int16_t (°C * 10)
    RSSI            = 0x43,     // int8_t (dBm)
    
    // Status (0x50-0x5F)
    CONNECTION      = 0x50,     // uint8_t (0=disconnected, 1=connected)
    MODE            = 0x51,     // uint8_t
    
    // Sensoren (0x60-0x6F)
    DISTANCE        = 0x60,     // uint16_t (mm)
    ACCELERATION    = 0x61,     // struct { int16_t x, y, z; }
    GYROSCOPE       = 0x62,     // struct { int16_t x, y, z; }
    
    // Raw Data (0xF0-0xFF)
    RAW_DATA_1      = 0xF0,
    RAW_DATA_2      = 0xF1,
    RAW_DATA_3      = 0xF2,
    RAW_DATA        = 0xFF      // Beliebige Rohdaten
};

// ═══════════════════════════════════════════════════════════════════════════
// ESPNOW PACKET
// ═══════════════════════════════════════════════════════════════════════════

/**
 * ESP-NOW Paket mit Builder-Pattern und Parser
 */
class ESPNowPacket {
public:
    ESPNowPacket();
    virtual ~ESPNowPacket();
    
    // ═══════════════════════════════════════════════════════════════════════
    // BUILDER
    // ═══════════════════════════════════════════════════════════════════════
    
    ESPNowPacket& begin(MainCmd cmd);
    ESPNowPacket& add(DataCmd dataCmd, const void* data, size_t len);
    ESPNowPacket& addByte(DataCmd dataCmd, uint8_t value);
    ESPNowPacket& addInt8(DataCmd dataCmd, int8_t value);
    ESPNowPacket& addUInt16(DataCmd dataCmd, uint16_t value);
    ESPNowPacket& addInt16(DataCmd dataCmd, int16_t value);
    ESPNowPacket& addUInt32(DataCmd dataCmd, uint32_t value);
    ESPNowPacket& addInt32(DataCmd dataCmd, int32_t value);
    ESPNowPacket& addFloat(DataCmd dataCmd, float value);
    
    template<typename T>
    ESPNowPacket& addStruct(DataCmd dataCmd, const T& data) {
        return add(dataCmd, &data, sizeof(T));
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // PARSER
    // ═══════════════════════════════════════════════════════════════════════
    
    bool parse(const uint8_t* rawData, size_t len);
    bool has(DataCmd dataCmd) const;
    const uint8_t* getData(DataCmd dataCmd, size_t* outLen = nullptr) const;
    
    template<typename T>
    const T* get(DataCmd dataCmd) const {
        size_t len;
        const uint8_t* data = getData(dataCmd, &len);
        if (data && len >= sizeof(T)) {
            return reinterpret_cast<const T*>(data);
        }
        return nullptr;
    }
    
    bool getByte(DataCmd dataCmd, uint8_t& outValue) const;
    bool getInt8(DataCmd dataCmd, int8_t& outValue) const;
    bool getUInt16(DataCmd dataCmd, uint16_t& outValue) const;
    bool getInt16(DataCmd dataCmd, int16_t& outValue) const;
    bool getUInt32(DataCmd dataCmd, uint32_t& outValue) const;
    bool getInt32(DataCmd dataCmd, int32_t& outValue) const;
    bool getFloat(DataCmd dataCmd, float& outValue) const;
    
    // ═══════════════════════════════════════════════════════════════════════
    // GETTER
    // ═══════════════════════════════════════════════════════════════════════
    
    MainCmd getMainCmd() const { return mainCmd; }
    const uint8_t* getRawData() const { return buffer; }
    size_t getTotalLength() const { return 2 + dataLength; }
    size_t getDataLength() const { return dataLength; }
    int getEntryCount() const { return entryCount; }
    bool isValid() const { return valid; }
    
    void clear();
    void print() const;

protected:
    uint8_t buffer[ESPNOW_MAX_PACKET_SIZE];
    
    struct DataEntry {
        DataCmd cmd;
        uint8_t offset;
        uint8_t length;
    };
    static const int MAX_ENTRIES = 20;
    DataEntry entries[MAX_ENTRIES];
    int entryCount;
    
    MainCmd mainCmd;
    size_t dataLength;
    size_t writePos;
    bool valid;
    
    int findEntry(DataCmd cmd) const;
};

#endif