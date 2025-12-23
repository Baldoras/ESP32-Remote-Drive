/**
 * UserConfig.cpp
 * 
 * Implementation der UserConfig Interface-Klasse
 */

#include "include/UserConfig.h"
#include "include/userConf.h"
#include "include/setupConf.h"

UserConfig::UserConfig()
    : ConfigManager()
{
    // Defaults initialisieren
    initDefaults();
    
    // Config auf Defaults setzen
    memcpy(&config, &defaults, sizeof(UserConfigStruct));
}

UserConfig::~UserConfig() {
}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC INTERFACE
// ═══════════════════════════════════════════════════════════════════════════

bool UserConfig::init(const char* configPath, SDCardHandler* sdHandler) {
    DEBUG_PRINTLN("UserConfig: Initialisiere...");
    
    // SDCardHandler setzen
    setSDCardHandler(sdHandler);
    
    // Config-Pfad setzen
    setConfigPath(configPath);
    
    DEBUG_PRINTLN("UserConfig: ✅ Initialisiert");
    return true;
}

bool UserConfig::load() {
    DEBUG_PRINTLN("UserConfig: Lade Config...");
    
    // 1. Aus Storage laden
    String content;
    if (!loadFromStorage(content)) {
        DEBUG_PRINTLN("UserConfig: ⚠️ Keine Config gefunden, verwende Defaults");
        reset();
        return false;
    }
    
    // 2. Scheme aufbauen
    ConfigScheme scheme = buildScheme();
    
    // 3. JSON deserialisieren
    if (!deserializeFromJson(content, scheme)) {
        DEBUG_PRINTLN("UserConfig: ❌ JSON-Deserialisierung fehlgeschlagen");
        return false;
    }
    
    // 4. Validieren
    if (!validate()) {
        DEBUG_PRINTLN("UserConfig: ⚠️ Werte korrigiert");
    }
    
    setDirty(false);
    
    DEBUG_PRINTLN("UserConfig: ✅ Config geladen");
    return true;
}

bool UserConfig::save() {
    DEBUG_PRINTLN("UserConfig: Speichere Config...");
    
    // 1. Validieren vor dem Speichern
    validate();
    
    // 2. Scheme aufbauen
    ConfigScheme scheme = buildScheme();
    
    // 3. Zu JSON serialisieren
    String content;
    if (!serializeToJson(content, scheme)) {
        DEBUG_PRINTLN("UserConfig: ❌ JSON-Serialisierung fehlgeschlagen");
        return false;
    }
    
    // 4. Zu Storage speichern
    if (!saveToStorage(content)) {
        DEBUG_PRINTLN("UserConfig: ❌ Speichern fehlgeschlagen");
        return false;
    }
    
    setDirty(false);
    
    DEBUG_PRINTLN("UserConfig: ✅ Config gespeichert");
    return true;
}

bool UserConfig::validate() {
    // Scheme aufbauen
    ConfigScheme scheme = buildScheme();
    
    // Generische Validierung der Basis-Klasse verwenden
    return ConfigManager::validate(scheme);
}

void UserConfig::reset() {
    DEBUG_PRINTLN("UserConfig: Setze auf Defaults zurück...");
    
    // Scheme aufbauen
    ConfigScheme scheme = buildScheme();
    
    // Defaults laden (aus Basis-Klasse)
    loadDefaults(scheme);
    
    setDirty(true);
    
    DEBUG_PRINTLN("UserConfig: ✅ Defaults geladen");
}

void UserConfig::printInfo() const {
    DEBUG_PRINTLN("═══════════════════════════════════════════════════════");
    DEBUG_PRINTLN("UserConfig - Aktuelle Werte:");
    DEBUG_PRINTLN("═══════════════════════════════════════════════════════");
    
    // ESP-NOW
    DEBUG_PRINTLN("[ESP-NOW]");
    DEBUG_PRINTF("  espnowChannel: %d\n", config.espnowChannel);
    DEBUG_PRINTF("  espnowMaxPeers: %d\n", config.espnowMaxPeers);
    DEBUG_PRINTF("  espnowHeartbeat: %lu ms\n", config.espnowHeartbeat);
    DEBUG_PRINTF("  espnowTimeout: %lu ms\n", config.espnowTimeout);
    DEBUG_PRINTF("  espnowPeerMac: %s\n", config.espnowPeerMac);
        
    // Power
    DEBUG_PRINTLN("[Power]");
    DEBUG_PRINTF("  autoShutdownEnabled: %s\n", config.autoShutdownEnabled ? "true" : "false");
    
    // Debug
    DEBUG_PRINTLN("[Debug]");
    DEBUG_PRINTF("  debugSerialEnabled: %s\n", config.debugSerialEnabled ? "true" : "false");
    
    DEBUG_PRINTLN("═══════════════════════════════════════════════════════");
}

ConfigScheme UserConfig::getConfigScheme() {
    return buildScheme();
}

// ═══════════════════════════════════════════════════════════════════════════
// SETTER (mit Dirty-Tracking)
// ═══════════════════════════════════════════════════════════════════════════

void UserConfig::setEspnowChannel(uint8_t value) {
    config.espnowChannel = value;
    setDirty(true);
}

void UserConfig::setEspnowMaxPeers(uint8_t value) {
    config.espnowMaxPeers = value;
    setDirty(true);
}

void UserConfig::setEspnowHeartbeat(uint32_t value) {
    config.espnowHeartbeat = value;
    setDirty(true);
}

void UserConfig::setEspnowTimeout(uint32_t value) {
    config.espnowTimeout = value;
    setDirty(true);
}

void UserConfig::setEspnowPeerMac(const char* mac) {
    if (mac) {
        strncpy(config.espnowPeerMac, mac, sizeof(config.espnowPeerMac) - 1);
        config.espnowPeerMac[sizeof(config.espnowPeerMac) - 1] = '\0';
        setDirty(true);
    }
}

void UserConfig::setAutoShutdownEnabled(bool value) {
    config.autoShutdownEnabled = value;
    setDirty(true);
}

void UserConfig::setDebugSerialEnabled(bool value) {
    config.debugSerialEnabled = value;
    setDirty(true);
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE - Scheme Definition
// ═══════════════════════════════════════════════════════════════════════════

ConfigScheme UserConfig::buildScheme() {
    // Statisches Array mit allen Config-Items
    static ConfigItem items[] = {
                // ESP-NOW
        {
            .key = "espnowChannel",
            .category = "ESP-Now",
            .type = ConfigType::UINT8,
            .valuePtr = &config.espnowChannel,
            .defaultPtr = &defaults.espnowChannel,
            .hasRange = true,
            .minValue = 0,
            .maxValue = 14,
            .maxLength = 0
        },
        {
            .key = "espnowMaxPeers",
            .category = "ESP-Now",
            .type = ConfigType::UINT8,
            .valuePtr = &config.espnowMaxPeers,
            .defaultPtr = &defaults.espnowMaxPeers,
            .hasRange = true,
            .minValue = 1,
            .maxValue = 20,
            .maxLength = 0
        },
        {
            .key = "espnowHeartbeat",
            .category = "ESP-Now",
            .type = ConfigType::UINT32,
            .valuePtr = &config.espnowHeartbeat,
            .defaultPtr = &defaults.espnowHeartbeat,
            .hasRange = true,
            .minValue = 100,
            .maxValue = 10000,
            .maxLength = 0
        },
        {
            .key = "espnowTimeout",
            .category = "ESP-Now",
            .type = ConfigType::UINT32,
            .valuePtr = &config.espnowTimeout,
            .defaultPtr = &defaults.espnowTimeout,
            .hasRange = true,
            .minValue = 500,
            .maxValue = 30000,
            .maxLength = 0
        },
        {
            .key = "espnowPeerMac",
            .category = "ESP-Now",
            .type = ConfigType::STRING,
            .valuePtr = &config.espnowPeerMac,
            .defaultPtr = &defaults.espnowPeerMac,
            .hasRange = false,
            .minValue = 0,
            .maxValue = 0,
            .maxLength = sizeof(config.espnowPeerMac)
        },
        // Power
        {
            .key = "autoShutdownEnabled",
            .category = "Power",
            .type = ConfigType::BOOL,
            .valuePtr = &config.autoShutdownEnabled,
            .defaultPtr = &defaults.autoShutdownEnabled,
            .hasRange = false,
            .minValue = 0,
            .maxValue = 0,
            .maxLength = 0
        },
        
        // Debug
        {
            .key = "debugSerialEnabled",
            .category = "Debug",
            .type = ConfigType::BOOL,
            .valuePtr = &config.debugSerialEnabled,
            .defaultPtr = &defaults.debugSerialEnabled,
            .hasRange = false,
            .minValue = 0,
            .maxValue = 0,
            .maxLength = 0
        }
    };
    
    // Scheme zurückgeben
    ConfigScheme scheme;
    scheme.items = items;
    scheme.count = sizeof(items) / sizeof(ConfigItem);
    
    return scheme;
}

void UserConfig::initDefaults() {
       
    // ESP-NOW
    defaults.espnowChannel = ESPNOW_CHANNEL;
    defaults.espnowMaxPeers = ESPNOW_MAX_PEERS;
    defaults.espnowHeartbeat = ESPNOW_HEARTBEAT_INTERVAL;
    defaults.espnowTimeout = ESPNOW_TIMEOUT;
    strncpy(defaults.espnowPeerMac, ESPNOW_PEER_MAC, sizeof(defaults.espnowPeerMac) - 1);
    defaults.espnowPeerMac[sizeof(defaults.espnowPeerMac) - 1] = '\0';
     
    // Power
    defaults.autoShutdownEnabled = AUTO_SHUTDOWN;
    
    // Debug
    defaults.debugSerialEnabled = DEBUG_SERIAL;
}