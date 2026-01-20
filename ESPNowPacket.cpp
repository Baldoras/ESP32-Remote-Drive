/**
 * ESPNowPacket.cpp
 * 
 * Implementation der ESP-NOW Paket-Klasse
 */

#include "include/ESPNowPacket.h"

ESPNowPacket::ESPNowPacket()
    : entryCount(0)
    , mainCmd(MainCmd::NONE)
    , dataLength(0)
    , writePos(2)
    , valid(false)
{
    memset(buffer, 0, ESPNOW_MAX_PACKET_SIZE);
    memset(entries, 0, sizeof(entries));
}

ESPNowPacket::~ESPNowPacket() {
}

// ═══════════════════════════════════════════════════════════════════════════
// BUILDER
// ═══════════════════════════════════════════════════════════════════════════

ESPNowPacket& ESPNowPacket::begin(MainCmd cmd) {
    clear();
    mainCmd = cmd;
    buffer[0] = static_cast<uint8_t>(cmd);
    buffer[1] = 0;
    writePos = 2;
    dataLength = 0;
    valid = true;
    return *this;
}

ESPNowPacket& ESPNowPacket::add(DataCmd dataCmd, const void* data, size_t len) {
    if (!valid) return *this;
    
    if (writePos + 2 + len > ESPNOW_MAX_PACKET_SIZE) {
        DEBUG_PRINTLN("ESPNowPacket: Kein Platz mehr!");
        return *this;
    }
    
    if (entryCount >= MAX_ENTRIES) {
        DEBUG_PRINTLN("ESPNowPacket: Max Einträge erreicht!");
        return *this;
    }
    
    buffer[writePos++] = static_cast<uint8_t>(dataCmd);
    buffer[writePos++] = static_cast<uint8_t>(len);
    
    if (data && len > 0) {
        memcpy(&buffer[writePos], data, len);
    }
    
    entries[entryCount].cmd = dataCmd;
    entries[entryCount].offset = writePos - 2;
    entries[entryCount].length = len;
    entryCount++;
    
    writePos += len;
    dataLength = writePos - 2;
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

// ═══════════════════════════════════════════════════════════════════════════
// PARSER
// ═══════════════════════════════════════════════════════════════════════════

bool ESPNowPacket::parse(const uint8_t* rawData, size_t len) {
    clear();
    
    if (!rawData || len < 2) {
        DEBUG_PRINTLN("ESPNowPacket: Paket zu klein");
        return false;
    }
    
    mainCmd = static_cast<MainCmd>(rawData[0]);
    uint8_t totalLen = rawData[1];
    
    if (totalLen > len - 2) {
        DEBUG_PRINTF("ESPNowPacket: Ungültige Länge: %d > %d\n", totalLen, len - 2);
        return false;
    }
    
    memcpy(buffer, rawData, len);
    dataLength = totalLen;
    
    size_t pos = 2;
    while (pos + 2 <= 2 + totalLen) {
        DataCmd subCmd = static_cast<DataCmd>(buffer[pos]);
        uint8_t subLen = buffer[pos + 1];
        
        if (pos + 2 + subLen > 2 + totalLen) {
            DEBUG_PRINTLN("ESPNowPacket: Truncated sub-entry");
            break;
        }
        
        if (entryCount < MAX_ENTRIES) {
            entries[entryCount].cmd = subCmd;
            entries[entryCount].offset = pos;
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

// ═══════════════════════════════════════════════════════════════════════════
// HELPER
// ═══════════════════════════════════════════════════════════════════════════

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
    DEBUG_PRINTLN("\n─── ESPNowPacket ───");
    DEBUG_PRINTF("MainCmd: 0x%02X\n", static_cast<uint8_t>(mainCmd));
    DEBUG_PRINTF("Length: %d\n", dataLength);
    DEBUG_PRINTF("Entries: %d\n", entryCount);
    DEBUG_PRINTF("Valid: %s\n", valid ? "YES" : "NO");
    
    for (int i = 0; i < entryCount; i++) {
        DEBUG_PRINTF("  [%d] Cmd=0x%02X, Len=%d\n", 
                     i, static_cast<uint8_t>(entries[i].cmd), entries[i].length);
    }
    
    DEBUG_PRINT("Raw: ");
    for (size_t i = 0; i < getTotalLength() && i < 32; i++) {
        DEBUG_PRINTF("%02X ", buffer[i]);
    }
    if (getTotalLength() > 32) DEBUG_PRINT("...");
    DEBUG_PRINTLN("\n────────────────────");
}