/**
 * SerialCommandHandler.h
 * 
 * Serial Command Interface für ESP32-S3 Remote Control
 * 
 * Features:
 * - Log-Dateien auflisten und lesen
 * - System-Informationen anzeigen
 * - Konfiguration anzeigen
 * - Log-Dateien löschen
 * 
 * Commands:
 *   help           - Zeigt alle verfügbaren Befehle
 *   logs           - Listet alle Log-Dateien auf
 *   read <file>    - Liest komplette Log-Datei
 *   tail <file> <n>- Zeigt letzte N Zeilen einer Log-Datei
 *   head <file> <n>- Zeigt erste N Zeilen einer Log-Datei
 *   clear <file>   - Löscht eine Log-Datei
 *   clearall       - Löscht alle Log-Dateien
 *   sysinfo        - Zeigt System-Informationen
 *   config         - Zeigt aktuelle Konfiguration
 *   battery        - Zeigt Battery-Status
 *   espnow         - Zeigt ESP-NOW Status
 */

#ifndef SERIAL_COMMAND_HANDLER_H
#define SERIAL_COMMAND_HANDLER_H

#include <Arduino.h>
#include "SDCardHandler.h"
#include "LogHandler.h"
#include "BatteryMonitor.h"
#include "ESPNowManager.h"
#include "UserConfig.h"
#include "Globals.h"

class SerialCommandHandler {
public:
    /**
     * Konstruktor
     */
    SerialCommandHandler();

    /**
     * Initialisieren
     * @param sdHandler Pointer zum SDCardHandler
     * @param logger Pointer zum LogHandler
     * @param battery Pointer zum BatteryMonitor (optional)
     * @param espNow Pointer zum ESPNowManager (optional)
     * @param config Pointer zum UserConfig (optional)
     */
    void begin(SDCardHandler* sdHandler, 
               LogHandler* logger,
               BatteryMonitor* battery = nullptr,
               ESPNowManager* espNow = nullptr,
               UserConfig* config = nullptr);

    /**
     * Update-Funktion (in loop() aufrufen)
     * Prüft auf eingehende Serial-Kommandos
     */
    void update();

    /**
     * Kommando manuell verarbeiten
     * @param cmd Command-String
     */
    void processCommand(const String& cmd);

private:
    SDCardHandler* sdHandler;
    LogHandler* logger;
    BatteryMonitor* battery;
    ESPNowManager* espNow;
    UserConfig* config;

    String inputBuffer;  // Buffer für eingehende Zeichen

    // Command-Handler Funktionen
    void handleHelp();
    void handleLogs();
    void handleRead(const String& filename);
    void handleTail(const String& filename, int lines);
    void handleHead(const String& filename, int lines);
    void handleClear(const String& filename);
    void handleClearAll();
    void handleSysInfo();
    void handleConfigList();
    void handleConfigGet(const String& key);
    void handleConfigSet(const String& key, const String& value);
    void handleConfigSave();
    void handleConfigReset();
    void handleBattery();
    void handleESPNow();

    // Hilfsfunktionen
    void listDirectory(const char* dirname);
    void readLogFile(const char* filepath);
    void readLogFileTail(const char* filepath, int lines);
    void readLogFileHead(const char* filepath, int lines);
    void printSeparator();
    void printHeader(const char* title);
    
    // Config-Hilfsfunktionen
    const ConfigItem* findConfigItem(const char* key);
    bool getConfigValueAsString(const ConfigItem* item, char* buffer, size_t bufferSize);
    bool setConfigValueFromString(const ConfigItem* item, const char* value);
};

#endif // SERIAL_COMMAND_HANDLER_H