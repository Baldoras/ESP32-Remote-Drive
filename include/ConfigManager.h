/**
 * ConfigManager.h
 * 
 * Generische Basis-Klasse für Konfigurations-Management
 * Wiederverwendbar für verschiedene Projekte
 * 
 * Verantwortlich für:
 * - Storage-Verwaltung (SD-Card)
 * - Backup/Restore
 * - JSON Serialisierung/Deserialisierung
 * - Generische Validierung
 * 
 * Abgeleitete Klassen definieren:
 * - Config-Schema (Struktur der Werte)
 * - Public Interface (save/load/validate)
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <map>
#include <string>

// Forward declaration
class SDCardHandler;

/**
 * Config-Datentypen
 */
enum class ConfigType {
    UINT8,
    UINT16,
    UINT32,
    INT16,
    INT32,
    BOOL,
    STRING,
    FLOAT
};

/**
 * Config-Schema Item
 * Definiert ein einzelnes Konfigurations-Feld
 */
struct ConfigItem {
    const char* key;        // JSON-Schlüssel
    const char* category;   // Kategorie (z.B. "Display", "Touch", "ESP-NOW")
    ConfigType type;        // Datentyp
    void* valuePtr;         // Pointer auf Wert in config struct
    void* defaultPtr;       // Pointer auf Default-Wert
    
    // Validierung (optional)
    bool hasRange;          // Hat Min/Max-Grenzen?
    float minValue;         // Minimum (falls hasRange)
    float maxValue;         // Maximum (falls hasRange)
    size_t maxLength;       // Max String-Länge (nur für STRING)
};

/**
 * Config-Schema
 * Array von ConfigItems
 */
struct ConfigScheme {
    const ConfigItem* items;
    size_t count;
};

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

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // STORAGE SETUP
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * SDCardHandler setzen
     * @param sdHandler Pointer zum SDCardHandler
     */
    void setSDCardHandler(SDCardHandler* sdHandler);
    
    /**
     * Ist Storage verfügbar? (SD-Card gemountet)
     * @return true wenn SDCardHandler verfügbar
     */
    bool isStorageAvailable() const;
    
    /**
     * Config-Pfade setzen
     * @param configPath Pfad zur Config-Datei
     */
    void setConfigPath(const char* configPath);

    // ═══════════════════════════════════════════════════════════════════════
    // BACKUP / RESTORE
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Backup erstellen (.bak Datei)
     * @return true bei Erfolg
     */
    bool createBackup();
    
    /**
     * Von Backup wiederherstellen
     * @return true bei Erfolg
     */
    bool restoreBackup();
    
    /**
     * Backup existiert?
     * @return true wenn Backup vorhanden
     */
    bool hasBackup() const;

    // ═══════════════════════════════════════════════════════════════════════
    // STORAGE OPERATIONS
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Config aus Storage laden (JSON String)
     * @param content Output: JSON-String aus Datei
     * @return true bei Erfolg
     */
    bool loadFromStorage(String& content);
    
    /**
     * Config zu Storage speichern (JSON String)
     * @param content JSON-String zum Speichern
     * @return true bei Erfolg
     */
    bool saveToStorage(const String& content);

    // ═══════════════════════════════════════════════════════════════════════
    // JSON SERIALIZATION
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Config-Daten aus JSON deserialisieren
     * Verwendet das Config-Schema zum Mapping
     * @param jsonString JSON als String
     * @param schema Config-Schema
     * @return true bei Erfolg
     */
    bool deserializeFromJson(const String& jsonString, const ConfigScheme& schema);
    
    /**
     * Config-Daten zu JSON serialisieren
     * Verwendet das Config-Schema zum Mapping
     * @param jsonString Output: JSON als String
     * @param schema Config-Schema
     * @return true bei Erfolg
     */
    bool serializeToJson(String& jsonString, const ConfigScheme& schema);

    // ═══════════════════════════════════════════════════════════════════════
    // VALIDATION
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Config-Daten validieren (generisch mit Schema)
     * Prüft Ranges, String-Längen, etc.
     * Korrigiert ungültige Werte automatisch
     * @param schema Config-Schema
     * @return true wenn gültig (oder korrigiert)
     */
    bool validate(const ConfigScheme& schema);

    // ═══════════════════════════════════════════════════════════════════════
    // DEFAULTS
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Config-Daten auf Defaults zurücksetzen
     * Kopiert Default-Werte aus Schema
     * @param schema Config-Schema
     */
    void loadDefaults(const ConfigScheme& schema);
    
    /**
     * Einzelnes Config-Item auf Default zurücksetzen
     * @param item Config-Item
     */
    void resetToDefault(const ConfigItem& item);

    // ═══════════════════════════════════════════════════════════════════════
    // HELPERS
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * Backup-Pfad generieren aus Config-Pfad
     */
    void generateBackupPath();
    
    /**
     * Dirty-Flag setzen
     */
    void setDirty(bool isDirty) { dirty = isDirty; }
    
    /**
     * Ist Config geändert?
     */
    bool isDirty() const { return dirty; }

private:
    // Storage
    SDCardHandler* sdCard;      // SD-Card Handler
    char configFilePath[64];    // Pfad zur Config-Datei
    char backupFilePath[64];    // Pfad zur Backup-Datei
    bool initialized;           // Init erfolgt?
    bool dirty;                 // Config geändert?
    
    /**
     * Wert aus JSON in Config-Item schreiben
     * @param item Config-Item
     * @param value JSON Variant
     * @return true bei Erfolg
     */
    bool setValueFromJson(const ConfigItem& item, const char* value);
    
    /**
     * Wert aus Config-Item in JSON schreiben
     * @param item Config-Item
     * @param buffer Output-Buffer für Wert-String
     * @param bufferSize Größe des Buffers
     * @return true bei Erfolg
     */
    bool getValueAsString(const ConfigItem& item, char* buffer, size_t bufferSize);
};

#endif // CONFIG_MANAGER_H