/**
 * SDCardHandler.h
 * 
 * SD-Karten Handler für ESP32-S3 - NUR File I/O
 * 
 * Features:
 * - Mount/Unmount auf VSPI
 * - Generische File-Operationen (read/write/append/delete)
 * - Kein Logging mehr (-> LogHandler)
 * - Thread-safe Operationen
 * 
 * REFACTORED: Logging komplett entfernt - nur noch reines File I/O!
 */

#ifndef SD_CARD_HANDLER_H
#define SD_CARD_HANDLER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include "setupConf.h"

class SDCardHandler {
public:
    /**
     * Konstruktor
     */
    SDCardHandler();

    /**
     * Destruktor
     */
    ~SDCardHandler();

    /**
     * SD-Karte initialisieren und mounten
     * @return true bei Erfolg
     */
    bool begin();

    /**
     * SD-Karte unmounten
     */
    void end();

    /**
     * Ist SD-Karte verfügbar?
     */
    bool isAvailable() const { return mounted; }

    /**
     * Freier Speicher in Bytes
     */
    uint64_t getFreeSpace();

    /**
     * Gesamter Speicher in Bytes
     */
    uint64_t getTotalSpace();

    /**
     * Verwendeter Speicher in Bytes
     */
    uint64_t getUsedSpace();

    // ═══════════════════════════════════════════════════════════════════════
    // FILE OPERATIONEN
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Datei schreiben (überschreiben)
     * @param path Dateipfad
     * @param data Daten
     * @return true bei Erfolg
     */
    bool writeFile(const char* path, const char* data);

    /**
     * Datei anhängen
     * @param path Dateipfad
     * @param data Daten
     * @return true bei Erfolg
     */
    bool appendFile(const char* path, const char* data);

    /**
     * Datei lesen (kompletter Inhalt)
     * @param path Dateipfad
     * @param buffer Buffer für Daten
     * @param maxLen Maximale Länge
     * @return Anzahl gelesener Bytes, -1 bei Fehler
     */
    int readFile(const char* path, char* buffer, size_t maxLen);

    /**
     * Datei lesen (als String)
     * @param path Dateipfad
     * @return Dateiinhalt als String (leer bei Fehler)
     */
    String readFileString(const char* path);

    /**
     * Binärdatei schreiben
     * @param path Dateipfad
     * @param data Daten
     * @param len Länge in Bytes
     * @return true bei Erfolg
     */
    bool writeBinaryFile(const char* path, const uint8_t* data, size_t len);

    /**
     * Binärdatei lesen
     * @param path Dateipfad
     * @param buffer Buffer für Daten
     * @param maxLen Maximale Länge
     * @return Anzahl gelesener Bytes, -1 bei Fehler
     */
    int readBinaryFile(const char* path, uint8_t* buffer, size_t maxLen);

    /**
     * Datei löschen
     * @param path Dateipfad
     * @return true bei Erfolg
     */
    bool deleteFile(const char* path);

    /**
     * Datei existiert?
     * @param path Dateipfad
     * @return true wenn vorhanden
     */
    bool fileExists(const char* path);

    /**
     * Dateigröße abrufen
     * @param path Dateipfad
     * @return Größe in Bytes, 0 bei Fehler
     */
    size_t getFileSize(const char* path);

    /**
     * Datei umbenennen/verschieben
     * @param oldPath Alter Pfad
     * @param newPath Neuer Pfad
     * @return true bei Erfolg
     */
    bool renameFile(const char* oldPath, const char* newPath);

    /**
     * Verzeichnis erstellen
     * @param path Verzeichnispfad
     * @return true bei Erfolg
     */
    bool createDir(const char* path);

    /**
     * Verzeichnis löschen (muss leer sein)
     * @param path Verzeichnispfad
     * @return true bei Erfolg
     */
    bool removeDir(const char* path);

    /**
     * Verzeichnis auflisten
     * @param path Verzeichnispfad
     * @param callback Callback-Funktion für jeden Eintrag
     */
    void listDir(const char* path, void (*callback)(const char* name, bool isDir, size_t size));

    /**
     * Manuelles Flush (alle offenen Dateien)
     */
    void flush();

    /**
     * Debug-Info ausgeben
     */
    void printInfo();

private:
    bool mounted;               // Mount-Status
    SPIClass* vspi;             // VSPI-Bus Pointer
    SemaphoreHandle_t mutex;    // Mutex für Thread-Safety
};

#endif // SD_CARD_HANDLER_H