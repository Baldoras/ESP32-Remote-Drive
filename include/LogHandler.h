/**
 * LogHandler.h
 * 
 * Linux-style Logging System für ESP32-S3 Remote Control
 * 
 * Features:
 * - Multiple Log-Levels (DEBUG, INFO, WARNING, ERROR)
 * - Separate Log-Dateien (battery.log, boot.log, connection.log, error.log)
 * - Dual-Output: Serial + SD-Karte (SD optional)
 * - Linux-style Format: [TIMESTAMP] [LEVEL] [TAG] Message
 * - Log-Rotation bei Dateigrößen-Limit
 * - Thread-safe Operationen
 * - Automatische Erstellung des /logs Verzeichnisses
 * 
 * Verwendung:
 *   logHandler.info("System", "Initialized successfully");
 *   logHandler.error("Touch", "Calibration failed", ERROR_TOUCH_CALIB);
 *   logHandler.logBattery(7.4, 85, false, false);
 */

#ifndef LOG_HANDLER_H
#define LOG_HANDLER_H

#include <Arduino.h>
#include "SDCardHandler.h"
#include "setupConf.h"  // Für LOGFILE_*, LOG_* und LOG_DIR Defines

// Log-Levels
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3
};

// Log-Kategorien (für separate Dateien)
enum LogCategory {
    LOG_CAT_GENERAL,    // allgemeine System-Logs
    LOG_CAT_BATTERY,    // battery.log
    LOG_CAT_BOOT,       // boot.log
    LOG_CAT_CONNECTION, // connection.log
    LOG_CAT_ERROR       // error.log (nur ERROR-Level)
};

class LogHandler {
public:
    /**
     * Konstruktor
     * @param sdHandler Pointer zum SDCardHandler (optional, nullptr = nur Serial)
     * @param minLevel Minimales Log-Level (alles darunter wird ignoriert)
     */
    LogHandler(SDCardHandler* sdHandler = nullptr, LogLevel minLevel = LOG_DEBUG);

    /**
     * Destruktor
     */
    ~LogHandler();

    // ═══════════════════════════════════════════════════════════════════════
    // ALLGEMEINE LOGGING-FUNKTIONEN
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Debug-Nachricht loggen
     * @param tag Modul/Tag (z.B. "Display", "Touch")
     * @param message Nachricht
     */
    void debug(const char* tag, const char* message);

    /**
     * Info-Nachricht loggen
     * @param tag Modul/Tag
     * @param message Nachricht
     */
    void info(const char* tag, const char* message);

    /**
     * Warning-Nachricht loggen
     * @param tag Modul/Tag
     * @param message Nachricht
     */
    void warning(const char* tag, const char* message);

    /**
     * Error-Nachricht loggen
     * @param tag Modul/Tag
     * @param message Nachricht
     * @param errorCode Optional: Fehlercode
     */
    void error(const char* tag, const char* message, int errorCode = 0);

    /**
     * Formatierte Log-Nachricht (printf-style)
     * @param level Log-Level
     * @param tag Modul/Tag
     * @param format Format-String (wie printf)
     * @param ... Variable Argumente
     */
    void logf(LogLevel level, const char* tag, const char* format, ...);

    // ═══════════════════════════════════════════════════════════════════════
    // SPEZIELLE LOG-FUNKTIONEN (für dedizierte Log-Dateien)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Boot-Start loggen
     * @param reason Reset-Grund (z.B. "PowerOn", "WatchdogReset")
     * @param freeHeap Freier Heap in Bytes
     * @param version Firmware-Version
     */
    void logBootStart(const char* reason, uint32_t freeHeap, const char* version);

    /**
     * Setup-Step loggen (während der Initialisierung)
     * @param module Modul-Name (z.B. "Display", "Touch", "ESP-NOW")
     * @param success Erfolgreich?
     * @param message Optionale Nachricht
     */
    void logBootStep(const char* module, bool success, const char* message = nullptr);

    /**
     * Boot-Complete loggen
     * @param totalTimeMs Gesamt-Initialisierungszeit in ms
     * @param success Alle Module erfolgreich?
     */
    void logBootComplete(uint32_t totalTimeMs, bool success);

    /**
     * Battery-Status loggen
     * @param voltage Spannung in Volt
     * @param percent Ladezustand in %
     * @param isLow Low-Battery Status
     * @param isCritical Critical-Battery Status
     */
    void logBattery(float voltage, uint8_t percent, bool isLow, bool isCritical);

    /**
     * ESP-NOW Verbindungs-Event loggen
     * @param peerMac Peer MAC-Adresse als String
     * @param event Event-Typ ("paired", "connected", "disconnected", "timeout")
     * @param rssi Signalstärke in dBm
     */
    void logConnection(const char* peerMac, const char* event, int8_t rssi = 0);

    /**
     * ESP-NOW Statistiken loggen
     * @param peerMac Peer MAC-Adresse als String
     * @param packetsSent Gesendete Pakete
     * @param packetsReceived Empfangene Pakete
     * @param packetsLost Verlorene Pakete
     * @param avgRssi Durchschnittlicher RSSI
     */
    void logConnectionStats(const char* peerMac, uint32_t packetsSent, 
                           uint32_t packetsReceived, uint32_t packetsLost, int8_t avgRssi);

    /**
     * Crash/Exception loggen
     * @param pc Program Counter
     * @param excvaddr Exception Address
     * @param exccause Exception Cause
     * @param stackTrace Stack-Trace (optional)
     */
    void logCrash(uint32_t pc, uint32_t excvaddr, uint32_t exccause, const char* stackTrace = nullptr);

    // ═══════════════════════════════════════════════════════════════════════
    // KONFIGURATION & VERWALTUNG
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Minimales Log-Level setzen
     * @param level Neues minimales Level
     */
    void setMinLevel(LogLevel level) { minLevel = level; }

    /**
     * SD-Handler setzen (für spätere Initialisierung)
     * @param sdHandler Pointer zum SDCardHandler
     */
    void setSDHandler(SDCardHandler* sdHandler) { 
        this->sdHandler = sdHandler; 
        if (sdHandler && sdHandler->isAvailable()) {
            ensureLogDirectory();
        }
    }

    /**
     * Ist SD-Karte verfügbar?
     */
    bool hasSDCard() const { return sdHandler && sdHandler->isAvailable(); }

    /**
     * Alle Log-Dateien löschen
     */
    void clearAllLogs();

    /**
     * Debug-Info ausgeben
     */
    void printInfo();

private:
    SDCardHandler* sdHandler;  // Pointer zum SD-Handler (optional)
    LogLevel minLevel;         // Minimales Log-Level
    SemaphoreHandle_t mutex;   // Mutex für Thread-Safety

    /**
     * Interne Log-Funktion (Kern-Implementierung)
     * @param level Log-Level
     * @param category Log-Kategorie (bestimmt Datei)
     * @param tag Modul/Tag
     * @param message Nachricht
     */
    void log(LogLevel level, LogCategory category, const char* tag, const char* message);

    /**
     * Zeitstempel generieren
     * @param buffer Ausgabe-Buffer
     * @param bufferSize Größe des Buffers
     */
    void getTimestamp(char* buffer, size_t bufferSize);

    /**
     * Log-Level als String
     * @param level Log-Level
     * @return Level-String ("DEBUG", "INFO", "WARNING", "ERROR")
     */
    const char* levelToString(LogLevel level);

    /**
     * Kategorie zu Dateiname
     * @param category Log-Kategorie
     * @return Dateipfad
     */
    const char* categoryToFilename(LogCategory category);

    /**
     * Log-Nachricht an Serial ausgeben
     * @param levelStr Level-String
     * @param tag Tag
     * @param message Nachricht
     */
    void outputToSerial(const char* timestamp, const char* levelStr, 
                       const char* tag, const char* message);

    /**
     * Log-Nachricht an SD-Karte schreiben
     * @param category Log-Kategorie
     * @param levelStr Level-String
     * @param tag Tag
     * @param message Nachricht
     */
    void outputToSD(LogCategory category, const char* timestamp, 
                   const char* levelStr, const char* tag, const char* message);

    /**
     * Log-Rotation durchführen wenn nötig
     * @param filepath Dateipfad
     */
    void rotateLogIfNeeded(const char* filepath);

    /**
     * Log-Verzeichnis erstellen wenn nicht vorhanden
     */
    void ensureLogDirectory();
};

#endif // LOG_HANDLER_H