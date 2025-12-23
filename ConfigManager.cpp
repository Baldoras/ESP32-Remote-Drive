/**
 * ConfigManager.cpp
 * 
 * Implementation der generischen ConfigManager Basis-Klasse
 */

#include "include/ConfigManager.h"
#include "include/SDCardHandler.h"
#include <ArduinoJson.h>

// Debug-Makros (falls nicht in setupConf.h definiert)
#ifndef DEBUG_PRINTLN
    #define DEBUG_PRINTLN(x) Serial.println(x)
#endif
#ifndef DEBUG_PRINTF
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#endif

ConfigManager::ConfigManager()
    : sdCard(nullptr)
    , initialized(false)
    , dirty(false)
{
    memset(configFilePath, 0, sizeof(configFilePath));
    memset(backupFilePath, 0, sizeof(backupFilePath));
}

ConfigManager::~ConfigManager() {
    // SDCardHandler wird nicht gelöscht (Ownership extern)
}

// ═══════════════════════════════════════════════════════════════════════════
// STORAGE SETUP
// ═══════════════════════════════════════════════════════════════════════════

void ConfigManager::setSDCardHandler(SDCardHandler* sdHandler) {
    sdCard = sdHandler;
    DEBUG_PRINTLN("ConfigManager: SDCardHandler gesetzt");
}

bool ConfigManager::isStorageAvailable() const {
    return (sdCard != nullptr && sdCard->isAvailable());
}

void ConfigManager::setConfigPath(const char* configPath) {
    if (!configPath) {
        DEBUG_PRINTLN("ConfigManager: ❌ Ungültiger Config-Pfad");
        return;
    }
    
    strncpy(configFilePath, configPath, sizeof(configFilePath) - 1);
    configFilePath[sizeof(configFilePath) - 1] = '\0';
    
    generateBackupPath();
    initialized = true;
    
    DEBUG_PRINTF("ConfigManager: ✅ Config-Pfad: %s\n", configFilePath);
}

// ═══════════════════════════════════════════════════════════════════════════
// BACKUP / RESTORE
// ═══════════════════════════════════════════════════════════════════════════

bool ConfigManager::createBackup() {
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("ConfigManager: ❌ Storage nicht verfügbar");
        return false;
    }
    
    if (!initialized) {
        DEBUG_PRINTLN("ConfigManager: ❌ Config-Pfad nicht gesetzt");
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: Erstelle Backup: %s\n", backupFilePath);
    
    // Config-Datei lesen
    String content = sdCard->readFileString(configFilePath);
    if (content.length() == 0) {
        DEBUG_PRINTLN("ConfigManager: ❌ Config-Datei leer oder nicht lesbar");
        return false;
    }
    
    // Zu Backup-Datei schreiben
    if (!sdCard->writeFile(backupFilePath, content.c_str())) {
        DEBUG_PRINTLN("ConfigManager: ❌ Backup schreiben fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTLN("ConfigManager: ✅ Backup erstellt");
    return true;
}

bool ConfigManager::restoreBackup() {
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("ConfigManager: ❌ Storage nicht verfügbar");
        return false;
    }
    
    if (!initialized) {
        DEBUG_PRINTLN("ConfigManager: ❌ Config-Pfad nicht gesetzt");
        return false;
    }
    
    if (!sdCard->fileExists(backupFilePath)) {
        DEBUG_PRINTLN("ConfigManager: ❌ Backup-Datei existiert nicht");
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: Stelle Backup wieder her: %s\n", backupFilePath);
    
    // Backup-Datei lesen
    String content = sdCard->readFileString(backupFilePath);
    if (content.length() == 0) {
        DEBUG_PRINTLN("ConfigManager: ❌ Backup-Datei leer oder nicht lesbar");
        return false;
    }
    
    // Zu Config-Datei schreiben
    if (!sdCard->writeFile(configFilePath, content.c_str())) {
        DEBUG_PRINTLN("ConfigManager: ❌ Config wiederherstellen fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTLN("ConfigManager: ✅ Backup wiederhergestellt");
    return true;
}

bool ConfigManager::hasBackup() const {
    if (!isStorageAvailable()) return false;
    if (!initialized) return false;
    return sdCard->fileExists(backupFilePath);
}

// ═══════════════════════════════════════════════════════════════════════════
// STORAGE OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

bool ConfigManager::loadFromStorage(String& content) {
    if (!initialized) {
        DEBUG_PRINTLN("ConfigManager: ❌ Nicht initialisiert - setConfigPath() aufrufen!");
        return false;
    }
    
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("ConfigManager: ⚠️ Storage nicht verfügbar");
        return false;
    }
    
    if (!sdCard->fileExists(configFilePath)) {
        DEBUG_PRINTF("ConfigManager: ⚠️ Config-Datei nicht gefunden: %s\n", configFilePath);
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: Lade von Storage: %s\n", configFilePath);
    
    // Datei lesen
    content = sdCard->readFileString(configFilePath);
    
    if (content.length() == 0) {
        DEBUG_PRINTLN("ConfigManager: ❌ Datei leer oder Lesefehler");
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: ✅ %d Bytes gelesen\n", content.length());
    return true;
}

bool ConfigManager::saveToStorage(const String& content) {
    if (!initialized) {
        DEBUG_PRINTLN("ConfigManager: ❌ Nicht initialisiert - setConfigPath() aufrufen!");
        return false;
    }
    
    if (!isStorageAvailable()) {
        DEBUG_PRINTLN("ConfigManager: ❌ Storage nicht verfügbar");
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: Speichere zu Storage: %s\n", configFilePath);
    
    // Datei schreiben
    if (!sdCard->writeFile(configFilePath, content.c_str())) {
        DEBUG_PRINTLN("ConfigManager: ❌ Schreiben fehlgeschlagen");
        return false;
    }
    
    DEBUG_PRINTF("ConfigManager: ✅ %d Bytes geschrieben\n", content.length());
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// JSON SERIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

bool ConfigManager::deserializeFromJson(const String& jsonString, const ConfigScheme& scheme) {
    DEBUG_PRINTLN("ConfigManager: Deserialisiere JSON...");
    
    // JSON Document erstellen
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        DEBUG_PRINTF("ConfigManager: ❌ JSON Parse Error: %s\n", error.c_str());
        return false;
    }
    
    // Alle Schema-Items durchgehen
    int loaded = 0;
    for (size_t i = 0; i < scheme.count; i++) {
        const ConfigItem& item = scheme.items[i];
        
        // Key im JSON vorhanden?
        if (!doc[item.key].is<JsonVariant>()) {
            DEBUG_PRINTF("ConfigManager: ⚠️ Key nicht gefunden: %s\n", item.key);
            continue;
        }
        
        // Wert extrahieren und setzen
        const char* value = doc[item.key];
        if (setValueFromJson(item, value)) {
            loaded++;
        }
    }
    
    DEBUG_PRINTF("ConfigManager: ✅ %d/%d Werte geladen\n", loaded, scheme.count);
    return (loaded > 0);
}

bool ConfigManager::serializeToJson(String& jsonString, const ConfigScheme& scheme) {
    DEBUG_PRINTLN("ConfigManager: Serialisiere zu JSON...");
    
    // JSON Document erstellen
    JsonDocument doc;
    
    // Alle Schema-Items durchgehen
    char buffer[256];
    int saved = 0;
    
    for (size_t i = 0; i < scheme.count; i++) {
        const ConfigItem& item = scheme.items[i];
        
        // Wert als String holen
        if (getValueAsString(item, buffer, sizeof(buffer))) {
            doc[item.key] = buffer;
            saved++;
        }
    }
    
    // JSON zu String
    jsonString = "";
    serializeJson(doc, jsonString);
    
    DEBUG_PRINTF("ConfigManager: ✅ %d/%d Werte serialisiert (%d Bytes)\n", 
                 saved, scheme.count, jsonString.length());
    
    return (saved > 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// VALIDATION
// ═══════════════════════════════════════════════════════════════════════════

bool ConfigManager::validate(const ConfigScheme& scheme) {
    DEBUG_PRINTLN("ConfigManager: Validiere Config...");
    
    bool allValid = true;
    int corrected = 0;
    
    for (size_t i = 0; i < scheme.count; i++) {
        const ConfigItem& item = scheme.items[i];
        
        // Range-Check (falls definiert)
        if (item.hasRange) {
            float value = 0.0f;
            
            // Wert je nach Type auslesen
            switch (item.type) {
                case ConfigType::UINT8:
                    value = *(uint8_t*)item.valuePtr;
                    break;
                case ConfigType::UINT16:
                    value = *(uint16_t*)item.valuePtr;
                    break;
                case ConfigType::UINT32:
                    value = *(uint32_t*)item.valuePtr;
                    break;
                case ConfigType::INT16:
                    value = *(int16_t*)item.valuePtr;
                    break;
                case ConfigType::INT32:
                    value = *(int32_t*)item.valuePtr;
                    break;
                case ConfigType::FLOAT:
                    value = *(float*)item.valuePtr;
                    break;
                default:
                    continue; // Keine numerische Range-Check
            }
            
            // Range prüfen
            if (value < item.minValue || value > item.maxValue) {
                DEBUG_PRINTF("ConfigManager: ⚠️ %s außerhalb Range [%.1f-%.1f], korrigiere...\n",
                           item.key, item.minValue, item.maxValue);
                
                // Auf Default zurücksetzen
                resetToDefault(item);
                corrected++;
                allValid = false;
            }
        }
        
        // String-Länge prüfen
        if (item.type == ConfigType::STRING && item.maxLength > 0) {
            const char* str = (const char*)item.valuePtr;
            size_t len = strlen(str);
            
            if (len >= item.maxLength) {
                DEBUG_PRINTF("ConfigManager: ⚠️ %s zu lang (%d>=%d), kürze...\n",
                           item.key, len, item.maxLength);
                
                // String kürzen
                char* str_mut = (char*)item.valuePtr;
                str_mut[item.maxLength - 1] = '\0';
                corrected++;
                allValid = false;
            }
        }
    }
    
    if (corrected > 0) {
        DEBUG_PRINTF("ConfigManager: ⚠️ %d Werte korrigiert\n", corrected);
    } else {
        DEBUG_PRINTLN("ConfigManager: ✅ Alle Werte gültig");
    }
    
    return allValid;
}

// ═══════════════════════════════════════════════════════════════════════════
// DEFAULTS
// ═══════════════════════════════════════════════════════════════════════════

void ConfigManager::loadDefaults(const ConfigScheme& scheme) {
    DEBUG_PRINTLN("ConfigManager: Lade Defaults...");
    
    for (size_t i = 0; i < scheme.count; i++) {
        resetToDefault(scheme.items[i]);
    }
    
    DEBUG_PRINTF("ConfigManager: ✅ %d Defaults geladen\n", scheme.count);
}

void ConfigManager::resetToDefault(const ConfigItem& item) {
    if (!item.defaultPtr) return;
    
    // Je nach Type Default kopieren
    switch (item.type) {
        case ConfigType::UINT8:
            *(uint8_t*)item.valuePtr = *(uint8_t*)item.defaultPtr;
            break;
        case ConfigType::UINT16:
            *(uint16_t*)item.valuePtr = *(uint16_t*)item.defaultPtr;
            break;
        case ConfigType::UINT32:
            *(uint32_t*)item.valuePtr = *(uint32_t*)item.defaultPtr;
            break;
        case ConfigType::INT16:
            *(int16_t*)item.valuePtr = *(int16_t*)item.defaultPtr;
            break;
        case ConfigType::INT32:
            *(int32_t*)item.valuePtr = *(int32_t*)item.defaultPtr;
            break;
        case ConfigType::BOOL:
            *(bool*)item.valuePtr = *(bool*)item.defaultPtr;
            break;
        case ConfigType::FLOAT:
            *(float*)item.valuePtr = *(float*)item.defaultPtr;
            break;
        case ConfigType::STRING:
            strncpy((char*)item.valuePtr, (const char*)item.defaultPtr, item.maxLength - 1);
            ((char*)item.valuePtr)[item.maxLength - 1] = '\0';
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void ConfigManager::generateBackupPath() {
    // .bak Extension anhängen
    snprintf(backupFilePath, sizeof(backupFilePath), "%s.bak", configFilePath);
    DEBUG_PRINTF("ConfigManager: Backup-Pfad: %s\n", backupFilePath);
}

bool ConfigManager::setValueFromJson(const ConfigItem& item, const char* value) {
    if (!value) return false;
    
    // Je nach Type konvertieren und setzen
    switch (item.type) {
        case ConfigType::UINT8:
            *(uint8_t*)item.valuePtr = (uint8_t)atoi(value);
            return true;
            
        case ConfigType::UINT16:
            *(uint16_t*)item.valuePtr = (uint16_t)atoi(value);
            return true;
            
        case ConfigType::UINT32:
            *(uint32_t*)item.valuePtr = (uint32_t)strtoul(value, nullptr, 10);
            return true;
            
        case ConfigType::INT16:
            *(int16_t*)item.valuePtr = (int16_t)atoi(value);
            return true;
            
        case ConfigType::INT32:
            *(int32_t*)item.valuePtr = (int32_t)atol(value);
            return true;
            
        case ConfigType::BOOL:
            *(bool*)item.valuePtr = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            return true;
            
        case ConfigType::FLOAT:
            *(float*)item.valuePtr = atof(value);
            return true;
            
        case ConfigType::STRING:
            strncpy((char*)item.valuePtr, value, item.maxLength - 1);
            ((char*)item.valuePtr)[item.maxLength - 1] = '\0';
            return true;
    }
    
    return false;
}

bool ConfigManager::getValueAsString(const ConfigItem& item, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    
    // Je nach Type zu String konvertieren
    switch (item.type) {
        case ConfigType::UINT8:
            snprintf(buffer, bufferSize, "%u", *(uint8_t*)item.valuePtr);
            return true;
            
        case ConfigType::UINT16:
            snprintf(buffer, bufferSize, "%u", *(uint16_t*)item.valuePtr);
            return true;
            
        case ConfigType::UINT32:
            snprintf(buffer, bufferSize, "%lu", *(uint32_t*)item.valuePtr);
            return true;
            
        case ConfigType::INT16:
            snprintf(buffer, bufferSize, "%d", *(int16_t*)item.valuePtr);
            return true;
            
        case ConfigType::INT32:
            snprintf(buffer, bufferSize, "%ld", *(int32_t*)item.valuePtr);
            return true;
            
        case ConfigType::BOOL:
            snprintf(buffer, bufferSize, "%s", *(bool*)item.valuePtr ? "true" : "false");
            return true;
            
        case ConfigType::FLOAT:
            snprintf(buffer, bufferSize, "%.2f", *(float*)item.valuePtr);
            return true;
            
        case ConfigType::STRING:
            strncpy(buffer, (const char*)item.valuePtr, bufferSize - 1);
            buffer[bufferSize - 1] = '\0';
            return true;
    }
    
    return false;
}