/**
 * UserConfig.h
 * 
 * User-Konfiguration mit Scheme-basiertem Config-Management
 * Erbt von ConfigManager und definiert die Struktur der User-Settings
 * 
 * Verwendung:
 *   UserConfig uConf;
 *   uConf.init("/config.json", &sdCard);
 *   uConf.load();
 *   
 *   uint8_t brightness = uConf.getBacklightDefault();
 *   uConf.setBacklightDefault(128);
 *   uConf.save();
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "ConfigManager.h"

/**
 * UserConfigStruct - alle editierbaren User-Einstellungen
 */
struct UserConfigStruct {
        
    // ESP-NOW
    uint8_t espnowChannel;
    uint8_t espnowMaxPeers;
    uint32_t espnowHeartbeat;
    uint32_t espnowTimeout;
    char espnowPeerMac[18];  // "XX:XX:XX:XX:XX:XX"
    
    // Power
    bool autoShutdownEnabled;
    
    // Debug
    bool debugSerialEnabled;
};

class UserConfig : public ConfigManager {
public:
    /**
     * Konstruktor
     */
    UserConfig();
    
    /**
     * Destruktor
     */
    ~UserConfig();

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLIC INTERFACE
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * UserConfig initialisieren
     * @param configPath Pfad zur Config-Datei auf SD (z.B. "/config.json")
     * @param sdHandler Pointer zum SDCardHandler
     * @return true bei Erfolg
     */
    bool init(const char* configPath, SDCardHandler* sdHandler);
    
    /**
     * Config laden (aus Storage)
     * @return true bei Erfolg
     */
    bool load();
    
    /**
     * Config speichern (zu Storage)
     * @return true bei Erfolg
     */
    bool save();
    
    /**
     * Config validieren
     * Prüft alle Werte auf Gültigkeit und korrigiert falls nötig
     * @return true wenn gültig (oder korrigiert)
     */
    bool validate();
    
    /**
     * Config auf Defaults zurücksetzen
     */
    void reset();
    
    /**
     * Debug-Info ausgeben
     */
    void printInfo() const;

    ConfigScheme getConfigScheme() const;

    // ═══════════════════════════════════════════════════════════════════════
    // GETTER (Lesezugriff)
    // ═══════════════════════════════════════════════════════════════════════
        
    // ESP-NOW
    uint8_t getEspnowChannel() const { return config.espnowChannel; }
    uint8_t getEspnowMaxPeers() const { return config.espnowMaxPeers; }
    uint32_t getEspnowHeartbeat() const { return config.espnowHeartbeat; }
    uint32_t getEspnowTimeout() const { return config.espnowTimeout; }
    const char* getEspnowPeerMac() const { return config.espnowPeerMac; }
    
    // Power
    bool getAutoShutdownEnabled() const { return config.autoShutdownEnabled; }
    
    // Debug
    bool getDebugSerialEnabled() const { return config.debugSerialEnabled; }

    // ═══════════════════════════════════════════════════════════════════════
    // SETTER (Schreibzugriff mit Dirty-Tracking)
    // ═══════════════════════════════════════════════════════════════════════

    // ESP-NOW
    void setEspnowChannel(uint8_t value);
    void setEspnowMaxPeers(uint8_t value);
    void setEspnowHeartbeat(uint32_t value);
    void setEspnowTimeout(uint32_t value);
    void setEspnowPeerMac(const char* mac);
    
    // Power
    void setAutoShutdownEnabled(bool value);
    
    // Debug
    void setDebugSerialEnabled(bool value);

    ConfigScheme getConfigScheme();
    
private:
    UserConfigStruct config;          // Aktuelle Config-Werte
    UserConfigStruct defaults;        // Default-Werte
    
    /**
     * Config-Scheme aufbauen
     * Definiert Struktur und Metadaten aller Config-Items
     */
    ConfigScheme buildScheme();
    
    /**
     * Default-Werte aus userConf.h initialisieren
     */
    void initDefaults();
};

#endif // USER_CONFIG_H