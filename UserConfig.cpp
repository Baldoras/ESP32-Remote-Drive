/**
 * UserConfig.cpp
 * 
 * Implementation der User-Konfiguration
 */

#include "include/UserConfig.h"
#include "include/SDCardHandler.h"

UserConfig::UserConfig()
    : ConfigManager()
    , sdCard(nullptr)
    , initialized(false)
{
    memset(configFilePath, 0, sizeof(configFilePath));
    memset(backupFilePath, 0, sizeof(backupFilePath));
}

UserConfig::~UserConfig() {
    // SDCardHandler wird nicht gelöscht (Ownership extern)
}

bool UserConfig::init(const char* configPath, SDCardHandler* sdHandler) {
    if (!configPath) {
        DEBUG_PRINTLN("UserConfig: ❌ Ungültiger Config-Pfad");
        return false;
    }
    
    // Config-Pfad speichern
    strncpy(configFilePath, configPath, sizeof(configFilePath) - 1);
    configFilePath[sizeof(configFilePath) - 1] = '\0';
    
    // SDCardHandler setzen
    sdCard = sdHandler;
    
    // Backup-Pfad generieren
    generateBackupPath();
    
    initialized = true;
    
    DEBUG_PRINTF("UserConfig: ✅ Initialisiert mit Pfad: %s\n", configFilePath);
    return true;
}

void UserConfig::setSDCardHandler(SDCardHandler* sdHandler) {
    sdCard = sdHandler;
    DEBUG_PRINTLN("UserConfig: SDCardHandler gesetzt");
}

bool UserConfig::isStorageAvailable() const {
    return (sdCard != nullptr && sdCard->isAvailable());
}

// ═══════════════════════════════════════════════════════════════════════════
// BACKUP / RESTORE
// ═══════════════════════════════════════════════════════════════════════════

bool UserConfig::createBackup() {
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("UserConfig: ❌ SD-Card nicht verfügbar");
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: Erstelle Backup: %s\n", backupFilePath);
    
    // Config-Datei zu Backup kopieren
    String content = sdCard->readFileString(configFilePath);
    if (content.length() == 0) {
        DEBUG_PRINTLN("UserConfig: ❌ Config-Datei leer oder nicht lesbar");
        return false;
    }
    
    if (!sdCard->writeFile(backupFilePath, content.c_str())) {
        DEBUG_PRINTLN("UserConfig: ❌ Backup schreiben fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTLN("UserConfig: ✅ Backup erstellt");
    return true;
}

bool UserConfig::restoreBackup() {
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("UserConfig: ❌ SD-Card nicht verfügbar");
        return false;
    }
    
    if (!sdCard->fileExists(backupFilePath)) {
        DEBUG_PRINTLN("UserConfig: ❌ Backup-Datei nicht vorhanden");
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: Stelle Backup wieder her: %s\n", backupFilePath);
    
    // Backup zu Config kopieren
    String content = sdCard->readFileString(backupFilePath);
    if (content.length() == 0) {
        DEBUG_PRINTLN("UserConfig: ❌ Backup-Datei leer oder nicht lesbar");
        return false;
    }
    
    if (!sdCard->writeFile(configFilePath, content.c_str())) {
        DEBUG_PRINTLN("UserConfig: ❌ Config wiederherstellen fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTLN("UserConfig: ✅ Backup wiederhergestellt");
    
    // Config neu laden
    return load();
}

bool UserConfig::hasBackup() const {
    if (!isStorageAvailable()) return false;
    return sdCard->fileExists(backupFilePath);
}

// ═══════════════════════════════════════════════════════════════════════════
// PROTECTED - Storage Implementation
// ═══════════════════════════════════════════════════════════════════════════

bool UserConfig::loadFromStorage(String& content) {
    if (!initialized) {
        DEBUG_PRINTLN("UserConfig: ❌ Nicht initialisiert - init() aufrufen!");
        return false;
    }
    
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("UserConfig: ⚠️ SD-Card nicht verfügbar");
        return false;
    }
    
    if (!sdCard->fileExists(configFilePath)) {
        DEBUG_PRINTF("UserConfig: ⚠️ Config-Datei nicht gefunden: %s\n", configFilePath);
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: Lade von SD: %s\n", configFilePath);
    
    // Datei lesen
    content = sdCard->readFileString(configFilePath);
    
    if (content.length() == 0) {
        DEBUG_PRINTLN("UserConfig: ❌ Datei leer oder Lesefehler");
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: ✅ %d Bytes gelesen\n", content.length());
    return true;
}

bool UserConfig::saveToStorage(const String& content) {
    if (!initialized) {
        DEBUG_PRINTLN("UserConfig: ❌ Nicht initialisiert - init() aufrufen!");
        return false;
    }
    
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("UserConfig: ❌ SD-Card nicht verfügbar");
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: Speichere zu SD: %s\n", configFilePath);
    
    // Backup erstellen vor dem Überschreiben
    if (sdCard->fileExists(configFilePath)) {
        createBackup();
    }
    
    // Datei schreiben
    if (!sdCard->writeFile(configFilePath, content.c_str())) {
        DEBUG_PRINTLN("UserConfig: ❌ Schreiben fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTF("UserConfig: ✅ %d Bytes geschrieben\n", content.length());
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE
// ═══════════════════════════════════════════════════════════════════════════

void UserConfig::generateBackupPath() {
    // .bak Extension anhängen
    snprintf(backupFilePath, sizeof(backupFilePath), "%s.bak", configFilePath);
}

void UserConfig::loadDefaults() {
    // Display
    config.backlightDefault = BACKLIGHT_DEFAULT;
    
    // Touch
    config.touchMinX = TOUCH_MIN_X;
    config.touchMaxX = TOUCH_MAX_X;
    config.touchMinY = TOUCH_MIN_Y;
    config.touchMaxY = TOUCH_MAX_Y;
    config.touchThreshold = TOUCH_THRESHOLD;
    config.touchRotation = TOUCH_ROTATION;
    
    // ESP-NOW
    config.espnowHeartbeat = ESPNOW_HEARTBEAT_INTERVAL;
    config.espnowTimeout = ESPNOW_TIMEOUT_MS;
    strncpy(config.espnowPeerMac, ESPNOW_PEER_DEVICE_MAC, sizeof(config.espnowPeerMac) - 1);
    config.espnowChannel = ESPNOW_CHANNEL;
    config.espnowMaxPeers = ESPNOW_MAX_PEERS;
    config.espnowPeerMac[sizeof(config.espnowPeerMac) - 1] = '\0';
    
    // Joystick
    config.joyDeadzone = JOY_DEADZONE;
    config.joyUpdateInterval = JOY_UPDATE_INTERVAL;
    config.joyInvertX = JOY_INVERT_X;
    config.joyInvertY = JOY_INVERT_Y;
    
    // Joystick Kalibrierung
    config.joyCalXMin = JOY_CAL_X_MIN;
    config.joyCalXCenter = JOY_CAL_X_CENTER;
    config.joyCalXMax = JOY_CAL_X_MAX;
    config.joyCalYMin = JOY_CAL_Y_MIN;
    config.joyCalYCenter = JOY_CAL_Y_CENTER;
    config.joyCalYMax = JOY_CAL_Y_MAX;
    
    // Auto Shutdown
    config.autoShutdownEnabled = AUTO_SHUTDOWN;

    // Debug
    config.debugSerialEnabled = DEBUG_SERIAL;
}

// ═══════════════════════════════════════════════════════════════════════════
// OVERRIDE - ConfigManager Implementierung
// ═══════════════════════════════════════════════════════════════════════════

void UserConfig::reset() {
    DEBUG_PRINTLN("UserConfig: Setze auf Defaults zurück...");
    loadDefaults();
    setDirty(true);
    DEBUG_PRINTLN("UserConfig: ✅ Defaults geladen");
}

bool UserConfig::validate() {
    bool changed = false;
    
    // Display - Backlight (0-255)
    if (config.backlightDefault > 255) {
        config.backlightDefault = 255;
        changed = true;
    }
    
    // Touch - Rotation (0-3)
    if (config.touchRotation > 3) {
        config.touchRotation = TOUCH_ROTATION;
        changed = true;
    }
    
    // Touch - Threshold (1-255)
    if (config.touchThreshold == 0 || config.touchThreshold > 255) {
        config.touchThreshold = TOUCH_THRESHOLD;
        changed = true;
    }
    
    // ESP-NOW - Heartbeat (mindestens 100ms)
    if (config.espnowHeartbeat < 100) {
        config.espnowHeartbeat = 100;
        changed = true;
    }
    
    // ESP-NOW - Timeout (mindestens 500ms)
    if (config.espnowTimeout < 500) {
        config.espnowTimeout = 500;
    }
    
    // ESP-NOW - Channel (0-13)
    if (config.espnowChannel > 13) {
        config.espnowChannel = 0;
        changed = true;
    }
    
    // ESP-NOW - MaxPeers (1-20)
    if (config.espnowMaxPeers == 0 || config.espnowMaxPeers > 20) {
        config.espnowMaxPeers = ESPNOW_MAX_PEERS;
        changed = true;
        changed = true;
    }
    
    // Joystick - Deadzone (0-50%)
    if (config.joyDeadzone > 50) {
        config.joyDeadzone = 50;
        changed = true;
    }
    
    // Joystick - Update Interval (mindestens 10ms)
    if (config.joyUpdateInterval < 10) {
        config.joyUpdateInterval = 10;
        changed = true;
    }
    
    // Joystick Kalibrierung - Plausibilität prüfen
    if (config.joyCalXMin >= config.joyCalXCenter || config.joyCalXCenter >= config.joyCalXMax) {
        config.joyCalXMin = JOY_CAL_X_MIN;
        config.joyCalXCenter = JOY_CAL_X_CENTER;
        config.joyCalXMax = JOY_CAL_X_MAX;
        changed = true;
    }
    
    if (config.joyCalYMin >= config.joyCalYCenter || config.joyCalYCenter >= config.joyCalYMax) {
        config.joyCalYMin = JOY_CAL_Y_MIN;
        config.joyCalYCenter = JOY_CAL_Y_CENTER;
        config.joyCalYMax = JOY_CAL_Y_MAX;
        changed = true;
    }
    
    if (changed) {
        DEBUG_PRINTLN("UserConfig: ⚠️ Ungültige Werte korrigiert");
        setDirty(true);
    }
    
    return true;
}

void UserConfig::printInfo() const {
    DEBUG_PRINTLN("\n═══════════════════════════════════════════════");
    DEBUG_PRINTLN("📋 USER CONFIG");
    DEBUG_PRINTLN("═══════════════════════════════════════════════");
    
    DEBUG_PRINTLN("\n🖥️  DISPLAY:");
    DEBUG_PRINTF("  Backlight Default: %d\n", config.backlightDefault);
    
    DEBUG_PRINTLN("\n👆 TOUCH:");
    DEBUG_PRINTF("  Calibration X: %d - %d\n", config.touchMinX, config.touchMaxX);
    DEBUG_PRINTF("  Calibration Y: %d - %d\n", config.touchMinY, config.touchMaxY);
    DEBUG_PRINTF("  Threshold: %d\n", config.touchThreshold);
    DEBUG_PRINTF("  Rotation: %d\n", config.touchRotation);
    
    DEBUG_PRINTLN("\n📡 ESP-NOW:");
    DEBUG_PRINTF("  Heartbeat: %dms\n", config.espnowHeartbeat);
    DEBUG_PRINTF("  Timeout: %dms\n", config.espnowTimeout);
    DEBUG_PRINTF("  Peer MAC: %s\n", config.espnowPeerMac);
    DEBUG_PRINTF("  Channel: %d\n", config.espnowChannel);
    DEBUG_PRINTF("  Max Peers: %d\n", config.espnowMaxPeers);
    
    DEBUG_PRINTLN("\n🕹️  JOYSTICK:");
    DEBUG_PRINTF("  Deadzone: %d%%\n", config.joyDeadzone);
    DEBUG_PRINTF("  Update Interval: %dms\n", config.joyUpdateInterval);
    DEBUG_PRINTF("  Invert X: %s\n", config.joyInvertX ? "Yes" : "No");
    DEBUG_PRINTF("  Invert Y: %s\n", config.joyInvertY ? "Yes" : "No");
    
    DEBUG_PRINTLN("\n🎯 JOYSTICK CALIBRATION:");
    DEBUG_PRINTF("  X-Axis: %d | %d | %d\n", config.joyCalXMin, config.joyCalXCenter, config.joyCalXMax);
    DEBUG_PRINTF("  Y-Axis: %d | %d | %d\n", config.joyCalYMin, config.joyCalYCenter, config.joyCalYMax);
    
    DEBUG_PRINTLN("\n🔧 Auto-Shutdown:");
    DEBUG_PRINTF("  Auto-Shutdown: %s\n", config.autoShutdownEnabled ? "Enabled" : "Disabled");

    DEBUG_PRINTLN("\n🔧 DEBUG:");
    DEBUG_PRINTF("  Serial Debug: %s\n", config.debugSerialEnabled ? "Enabled" : "Disabled");
    
    DEBUG_PRINTLN("\n💾 STORAGE:");
    DEBUG_PRINTF("  Config Path: %s\n", configFilePath);
    DEBUG_PRINTF("  Backup Path: %s\n", backupFilePath);
    DEBUG_PRINTF("  Dirty Flag: %s\n", isDirty() ? "YES" : "NO");
    
    DEBUG_PRINTLN("═══════════════════════════════════════════════\n");
}

bool UserConfig::deserializeFromJson(const String& jsonString) {
    DEBUG_PRINTLN("UserConfig: Deserialisiere JSON...");
    
    // JSON-Dokument erstellen (2KB sollte ausreichen)
    JsonDocument doc;
    
    // JSON parsen
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        DEBUG_PRINTF("UserConfig: ❌ JSON Parse Error: %s\n", error.c_str());
        return false;
    }
    
    // Defaults laden (falls Werte fehlen)
    loadDefaults();
    
    // Display
    config.backlightDefault = doc["display"]["backlight"] | config.backlightDefault;
    
    // Touch
    config.touchMinX = doc["touch"]["minX"] | config.touchMinX;
    config.touchMaxX = doc["touch"]["maxX"] | config.touchMaxX;
    config.touchMinY = doc["touch"]["minY"] | config.touchMinY;
    config.touchMaxY = doc["touch"]["maxY"] | config.touchMaxY;
    config.touchThreshold = doc["touch"]["threshold"] | config.touchThreshold;
    config.touchRotation = doc["touch"]["rotation"] | config.touchRotation;
    
    // ESP-NOW
    config.espnowHeartbeat = doc["espnow"]["heartbeat"] | config.espnowHeartbeat;
    config.espnowTimeout = doc["espnow"]["timeout"] | config.espnowTimeout;
    const char* mac = doc["espnow"]["peerMac"] | config.espnowPeerMac;
    strncpy(config.espnowPeerMac, mac, sizeof(config.espnowPeerMac) - 1);
    config.espnowPeerMac[sizeof(config.espnowPeerMac) - 1] = '\0';
    config.espnowChannel = doc["espnow"]["channel"] | config.espnowChannel;
    config.espnowMaxPeers = doc["espnow"]["maxPeers"] | config.espnowMaxPeers;
    
    // Joystick
    config.joyDeadzone = doc["joystick"]["deadzone"] | config.joyDeadzone;
    config.joyUpdateInterval = doc["joystick"]["updateInterval"] | config.joyUpdateInterval;
    config.joyInvertX = doc["joystick"]["invertX"] | config.joyInvertX;
    config.joyInvertY = doc["joystick"]["invertY"] | config.joyInvertY;
    
    // Joystick Kalibrierung
    config.joyCalXMin = doc["joystick"]["cal"]["xMin"] | config.joyCalXMin;
    config.joyCalXCenter = doc["joystick"]["cal"]["xCenter"] | config.joyCalXCenter;
    config.joyCalXMax = doc["joystick"]["cal"]["xMax"] | config.joyCalXMax;
    config.joyCalYMin = doc["joystick"]["cal"]["yMin"] | config.joyCalYMin;
    config.joyCalYCenter = doc["joystick"]["cal"]["yCenter"] | config.joyCalYCenter;
    config.joyCalYMax = doc["joystick"]["cal"]["yMax"] | config.joyCalYMax;
    
    // Auto-Shutdown
    config.autoShutdownEnabled = doc["autoshutdown"]["autoShutdownEnabled"] | config.autoShutdownEnabled;

    // Debug
    config.debugSerialEnabled = doc["debug"]["serialEnabled"] | config.debugSerialEnabled;
    
    DEBUG_PRINTLN("UserConfig: ✅ JSON deserialisiert");
    return true;
}

bool UserConfig::serializeToJson(String& jsonString) {
    DEBUG_PRINTLN("UserConfig: Serialisiere zu JSON...");
    
    // JSON-Dokument erstellen
    JsonDocument doc;
    
    // Display
    doc["display"]["backlight"] = config.backlightDefault;
    
    // Touch
    doc["touch"]["minX"] = config.touchMinX;
    doc["touch"]["maxX"] = config.touchMaxX;
    doc["touch"]["minY"] = config.touchMinY;
    doc["touch"]["maxY"] = config.touchMaxY;
    doc["touch"]["threshold"] = config.touchThreshold;
    doc["touch"]["rotation"] = config.touchRotation;
    
    // ESP-NOW
    doc["espnow"]["heartbeat"] = config.espnowHeartbeat;
    doc["espnow"]["timeout"] = config.espnowTimeout;
    doc["espnow"]["peerMac"] = config.espnowPeerMac;
    doc["espnow"]["channel"] = config.espnowChannel;
    doc["espnow"]["maxPeers"] = config.espnowMaxPeers;
    
    // Joystick
    doc["joystick"]["deadzone"] = config.joyDeadzone;
    doc["joystick"]["updateInterval"] = config.joyUpdateInterval;
    doc["joystick"]["invertX"] = config.joyInvertX;
    doc["joystick"]["invertY"] = config.joyInvertY;
    
    // Joystick Kalibrierung
    doc["joystick"]["cal"]["xMin"] = config.joyCalXMin;
    doc["joystick"]["cal"]["xCenter"] = config.joyCalXCenter;
    doc["joystick"]["cal"]["xMax"] = config.joyCalXMax;
    doc["joystick"]["cal"]["yMin"] = config.joyCalYMin;
    doc["joystick"]["cal"]["yCenter"] = config.joyCalYCenter;
    doc["joystick"]["cal"]["yMax"] = config.joyCalYMax;
    
    // Autoshutdown
    doc["autoshutdown"]["autoShutdownEnabled"] = config.autoShutdownEnabled;

    // Debug
    doc["debug"]["serialEnabled"] = config.debugSerialEnabled;
    
    // Zu String serialisieren (formatiert mit Einrückung)
    if (serializeJsonPretty(doc, jsonString) == 0) {
        DEBUG_PRINTLN("UserConfig: ❌ Serialisierung fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTLN("UserConfig: ✅ JSON serialisiert");
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// SETTER mit Dirty-Tracking
// ═══════════════════════════════════════════════════════════════════════════

void UserConfig::setBacklightDefault(uint8_t value) {
    if (value > 255) value = 255;
    if (config.backlightDefault != value) {
        config.backlightDefault = value;
        setDirty(true);
    }
}

void UserConfig::setTouchCalibration(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY) {
    bool changed = false;
    
    if (config.touchMinX != minX) { config.touchMinX = minX; changed = true; }
    if (config.touchMaxX != maxX) { config.touchMaxX = maxX; changed = true; }
    if (config.touchMinY != minY) { config.touchMinY = minY; changed = true; }
    if (config.touchMaxY != maxY) { config.touchMaxY = maxY; changed = true; }
    
    if (changed) setDirty(true);
}

void UserConfig::setTouchThreshold(uint16_t value) {
    if (value == 0) value = 1;
    if (value > 255) value = 255;
    if (config.touchThreshold != value) {
        config.touchThreshold = value;
        setDirty(true);
    }
}

void UserConfig::setTouchRotation(uint8_t value) {
    if (value > 3) value = 3;
    if (config.touchRotation != value) {
        config.touchRotation = value;
        setDirty(true);
    }
}

void UserConfig::setEspnowHeartbeat(uint32_t value) {
    if (value < 100) value = 100;
    if (config.espnowHeartbeat != value) {
        config.espnowHeartbeat = value;
        setDirty(true);
    }
}

void UserConfig::setEspnowTimeout(uint32_t value) {
    if (value < 500) value = 500;
    if (config.espnowTimeout != value) {
        config.espnowTimeout = value;
        setDirty(true);
    }
}

void UserConfig::setEspnowPeerMac(const char* mac) {
    if (!mac) return;
    if (strcmp(config.espnowPeerMac, mac) != 0) {
        strncpy(config.espnowPeerMac, mac, sizeof(config.espnowPeerMac) - 1);
        config.espnowPeerMac[sizeof(config.espnowPeerMac) - 1] = '\0';
        setDirty(true);
    }
}

void UserConfig::setJoyDeadzone(uint8_t value) {
    if (value > 50) value = 50;
    if (config.joyDeadzone != value) {
        config.joyDeadzone = value;
        setDirty(true);
    }
}

void UserConfig::setJoyUpdateInterval(uint16_t value) {
    if (value < 10) value = 10;
    if (config.joyUpdateInterval != value) {
        config.joyUpdateInterval = value;
        setDirty(true);
    }
}

void UserConfig::setJoyInvertX(bool value) {
    if (config.joyInvertX != value) {
        config.joyInvertX = value;
        setDirty(true);
    }
}

void UserConfig::setJoyInvertY(bool value) {
    if (config.joyInvertY != value) {
        config.joyInvertY = value;
        setDirty(true);
    }
}

void UserConfig::setJoyCalibration(uint8_t axis, int16_t min, int16_t center, int16_t max) {
    bool changed = false;
    
    if (axis == 0) { // X-Achse
        if (config.joyCalXMin != min) { config.joyCalXMin = min; changed = true; }
        if (config.joyCalXCenter != center) { config.joyCalXCenter = center; changed = true; }
        if (config.joyCalXMax != max) { config.joyCalXMax = max; changed = true; }
    } else if (axis == 1) { // Y-Achse
        if (config.joyCalYMin != min) { config.joyCalYMin = min; changed = true; }
        if (config.joyCalYCenter != center) { config.joyCalYCenter = center; changed = true; }
        if (config.joyCalYMax != max) { config.joyCalYMax = max; changed = true; }
    }
    
    if (changed) setDirty(true);
}

void UserConfig::setAutoShutdownEnabled(bool value) {
    if (config.autoShutdownEnabled != value) {
        config.autoShutdownEnabled = value;
        setDirty(true);
    }
}

void UserConfig::setDebugSerialEnabled(bool value) {
    if (config.debugSerialEnabled != value) {
        config.debugSerialEnabled = value;
        setDirty(true);
    }
}
void UserConfig::setEspnowChannel(uint8_t value) {
    if (value > 13) value = 0;  // Auto bei ungültigem Wert
    if (config.espnowChannel != value) {
        config.espnowChannel = value;
        setDirty(true);
    }
}

void UserConfig::setEspnowMaxPeers(uint8_t value) {
    if (value == 0) value = 1;
    if (value > 20) value = 20;
    if (config.espnowMaxPeers != value) {
        config.espnowMaxPeers = value;
        setDirty(true);
    }
}