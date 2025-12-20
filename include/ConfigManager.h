/**
 * ConfigManager.h
 * 
 * Abstrakte Basis-Klasse für Konfigurations-Management
 * 
 * Verantwortlich für:
 * - Load/Save Pattern (Template Method)
 * - Dirty-Tracking
 * 
 * UserConfig und SystemConfig erben von dieser Klasse und implementieren:
 * - Ihre eigene Config-Struktur (UserConfigData / SystemConfigData)
 * - deserializeFromJson() / serializeToJson()
 * - validate()
 * - reset()
 * - Getter/Setter
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

class ConfigManager {
public:
    /**
     * Konstruktor
     */
    ConfigManager();
    
    /**
     * Destruktor
     */
    virtual ~ConfigManager();

    /**
     * Config laden (aus Storage)
     * Template Method Pattern:
     * 1. loadFromStorage() aufrufen
     * 2. deserializeFromJson() aufrufen (implementiert von Subklasse)
     * 3. validate() aufrufen (implementiert von Subklasse)
     * @return true bei Erfolg
     */
    bool load();

    /**
     * Config speichern (zu Storage)
     * Template Method Pattern:
     * 1. serializeToJson() aufrufen (implementiert von Subklasse)
     * 2. saveToStorage() aufrufen
     * @return true bei Erfolg
     */
    bool save();

    /**
     * Config auf Defaults zurücksetzen
     * MUSS von Subklasse implementiert werden
     */
    virtual void reset() = 0;

    /**
     * Config validieren
     * MUSS von Subklasse implementiert werden
     * @return true wenn gültig (oder korrigiert)
     */
    virtual bool validate() = 0;

    /**
     * Ist Config geändert (dirty)?
     */
    bool isDirty() const { return dirty; }

    /**
     * Dirty-Flag manuell setzen
     */
    void setDirty(bool isDirty) { dirty = isDirty; }

    /**
     * Debug-Info ausgeben
     * MUSS von Subklasse implementiert werden
     */
    virtual void printInfo() const = 0;

protected:
    bool dirty;                 // Wurde Config geändert?
    
    /**
     * Laden aus Storage (implementiert von UserConfig/SystemConfig)
     * @param content Output: Dateiinhalt als String
     * @return true bei Erfolg
     */
    virtual bool loadFromStorage(String& content) = 0;
    
    /**
     * Speichern zu Storage (implementiert von UserConfig/SystemConfig)
     * @param content JSON-String zum Speichern
     * @return true bei Erfolg
     */
    virtual bool saveToStorage(const String& content) = 0;
    
    /**
     * JSON zu Config deserialisieren
     * MUSS von Subklasse implementiert werden
     * @param jsonString JSON als String
     * @return true bei Erfolg
     */
    virtual bool deserializeFromJson(const String& jsonString) = 0;
    
    /**
     * Config zu JSON serialisieren
     * MUSS von Subklasse implementiert werden
     * @param jsonString Output String für JSON
     * @return true bei Erfolg
     */
    virtual bool serializeToJson(String& jsonString) = 0;
};

#endif // CONFIG_MANAGER_H