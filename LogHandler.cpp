/**
 * LogHandler.cpp
 * 
 * Implementation des Linux-style Logging-Systems
 */

#include "include/LogHandler.h"
#include <stdarg.h>
#include <sys/time.h>

LogHandler::LogHandler(SDCardHandler* sdHandler, LogLevel minLevel)
    : sdHandler(sdHandler)
    , minLevel(minLevel)
    , mutex(nullptr)
{
    // Mutex für Thread-Safety erstellen
    mutex = xSemaphoreCreateMutex();
    
    // Log-Verzeichnis erstellen wenn SD-Karte verfügbar
    if (sdHandler && sdHandler->isAvailable()) {
        ensureLogDirectory();
    }
}

LogHandler::~LogHandler() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ALLGEMEINE LOGGING-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════

void LogHandler::debug(const char* tag, const char* message) {
    log(LOG_DEBUG, LOG_CAT_GENERAL, tag, message);
}

void LogHandler::info(const char* tag, const char* message) {
    log(LOG_INFO, LOG_CAT_GENERAL, tag, message);
}

void LogHandler::warning(const char* tag, const char* message) {
    log(LOG_WARNING, LOG_CAT_GENERAL, tag, message);
}

void LogHandler::error(const char* tag, const char* message, int errorCode) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    
    if (errorCode != 0) {
        snprintf(buffer, sizeof(buffer), "%s (code=%d)", message, errorCode);
        log(LOG_ERROR, LOG_CAT_ERROR, tag, buffer);
    } else {
        log(LOG_ERROR, LOG_CAT_ERROR, tag, message);
    }
}

void LogHandler::logf(LogLevel level, const char* tag, const char* format, ...) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    LogCategory category = (level == LOG_ERROR) ? LOG_CAT_ERROR : LOG_CAT_GENERAL;
    log(level, category, tag, buffer);
}

// ═══════════════════════════════════════════════════════════════════════════
// SPEZIELLE LOG-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════

void LogHandler::logBootStart(const char* reason, uint32_t freeHeap, const char* version) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), 
             "Boot started: reason=%s, heap=%u bytes, version=%s, chip=%s, cpu=%dMHz",
             reason, freeHeap, version, ESP.getChipModel(), ESP.getCpuFreqMHz());
    
    log(LOG_INFO, LOG_CAT_BOOT, "BOOT", buffer);
}

void LogHandler::logBootStep(const char* module, bool success, const char* message) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    
    if (message) {
        snprintf(buffer, sizeof(buffer), "Init %s: %s - %s", 
                module, success ? "OK" : "FAILED", message);
    } else {
        snprintf(buffer, sizeof(buffer), "Init %s: %s", 
                module, success ? "OK" : "FAILED");
    }
    
    LogLevel level = success ? LOG_INFO : LOG_ERROR;
    log(level, LOG_CAT_BOOT, "BOOT", buffer);
}

void LogHandler::logBootComplete(uint32_t totalTimeMs, bool success) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), 
             "Boot %s in %u ms, free heap: %u bytes",
             success ? "completed" : "failed", totalTimeMs, ESP.getFreeHeap());
    
    LogLevel level = success ? LOG_INFO : LOG_ERROR;
    log(level, LOG_CAT_BOOT, "BOOT", buffer);
}

void LogHandler::logBattery(float voltage, uint8_t percent, bool isLow, bool isCritical) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), 
             "voltage=%.2fV, level=%u%%, low=%s, critical=%s",
             voltage, percent, 
             isLow ? "true" : "false", 
             isCritical ? "true" : "false");
    
    LogLevel level = isCritical ? LOG_ERROR : (isLow ? LOG_WARNING : LOG_INFO);
    log(level, LOG_CAT_BATTERY, "BATTERY", buffer);
}

void LogHandler::logConnection(const char* peerMac, const char* event, int8_t rssi) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    
    if (rssi != 0) {
        snprintf(buffer, sizeof(buffer), 
                "peer=%s, event=%s, rssi=%ddBm", 
                peerMac, event, rssi);
    } else {
        snprintf(buffer, sizeof(buffer), 
                "peer=%s, event=%s", 
                peerMac, event);
    }
    
    LogLevel level = LOG_INFO;
    if (strcmp(event, "disconnected") == 0 || strcmp(event, "timeout") == 0) {
        level = LOG_WARNING;
    }
    
    log(level, LOG_CAT_CONNECTION, "ESP-NOW", buffer);
}

void LogHandler::logConnectionStats(const char* peerMac, uint32_t packetsSent, 
                                    uint32_t packetsReceived, uint32_t packetsLost, 
                                    int8_t avgRssi) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), 
             "peer=%s, sent=%u, recv=%u, lost=%u, rssi=%ddBm",
             peerMac, packetsSent, packetsReceived, packetsLost, avgRssi);
    
    log(LOG_INFO, LOG_CAT_CONNECTION, "ESP-NOW", buffer);
}

void LogHandler::logCrash(uint32_t pc, uint32_t excvaddr, uint32_t exccause, 
                         const char* stackTrace) {
    char buffer[LOG_MAX_MESSAGE_LEN];
    
    if (stackTrace) {
        snprintf(buffer, sizeof(buffer), 
                "CRASH: pc=0x%08X, addr=0x%08X, cause=%u, trace=%s",
                pc, excvaddr, exccause, stackTrace);
    } else {
        snprintf(buffer, sizeof(buffer), 
                "CRASH: pc=0x%08X, addr=0x%08X, cause=%u",
                pc, excvaddr, exccause);
    }
    
    log(LOG_ERROR, LOG_CAT_ERROR, "SYSTEM", buffer);
}

// ═══════════════════════════════════════════════════════════════════════════
// VERWALTUNGSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════

void LogHandler::clearAllLogs() {
    if (!hasSDCard()) {
        Serial.println("[LogHandler] No SD card available for clearing logs");
        return;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sdHandler->deleteFile(LOGFILE_BATTERY);
        sdHandler->deleteFile(LOGFILE_BOOT);
        sdHandler->deleteFile(LOGFILE_CONNECTION);
        sdHandler->deleteFile(LOGFILE_ERROR);
        
        // Rotierte Dateien auch löschen
        for (int i = 1; i <= LOG_ROTATION_KEEP; i++) {
            char rotatedPath[64];
            snprintf(rotatedPath, sizeof(rotatedPath), "%s.%d", LOGFILE_BATTERY, i);
            sdHandler->deleteFile(rotatedPath);
            
            snprintf(rotatedPath, sizeof(rotatedPath), "%s.%d", LOGFILE_BOOT, i);
            sdHandler->deleteFile(rotatedPath);
            
            snprintf(rotatedPath, sizeof(rotatedPath), "%s.%d", LOGFILE_CONNECTION, i);
            sdHandler->deleteFile(rotatedPath);
            
            snprintf(rotatedPath, sizeof(rotatedPath), "%s.%d", LOGFILE_ERROR, i);
            sdHandler->deleteFile(rotatedPath);
        }
        
        xSemaphoreGive(mutex);
        Serial.println("[LogHandler] All logs cleared");
    }
}

void LogHandler::printInfo() {
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.println("LogHandler Info:");
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.printf("  Min Level: %s\n", levelToString(minLevel));
    Serial.printf("  SD Card: %s\n", hasSDCard() ? "Available" : "Not available");
    Serial.printf("  Log Dir: %s\n", LOG_DIR);
    
    if (hasSDCard()) {
        Serial.println("\n  Log Files:");
        
        const char* files[] = {
            LOGFILE_BATTERY,
            LOGFILE_BOOT,
            LOGFILE_CONNECTION,
            LOGFILE_ERROR
        };
        
        for (int i = 0; i < 4; i++) {
            if (sdHandler->fileExists(files[i])) {
                size_t size = sdHandler->getFileSize(files[i]);
                Serial.printf("    %s: %.2f KB\n", files[i], size / 1024.0);
            } else {
                Serial.printf("    %s: not found\n", files[i]);
            }
        }
    }
    
    Serial.println("═══════════════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════

void LogHandler::ensureLogDirectory() {
    if (!hasSDCard()) return;
    
    if (!sdHandler->fileExists(LOG_DIR)) {
        if (sdHandler->createDir(LOG_DIR)) {
            Serial.printf("[LogHandler] Created log directory: %s\n", LOG_DIR);
        } else {
            Serial.printf("[LogHandler] Failed to create log directory: %s\n", LOG_DIR);
        }
    }
}

void LogHandler::log(LogLevel level, LogCategory category, const char* tag, const char* message) {
    // Level-Filter
    if (level < minLevel) return;
    
    // Thread-Safety
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[LogHandler] Failed to acquire mutex!");
        return;
    }
    
    // Timestamp generieren
    char timestamp[32];
    getTimestamp(timestamp, sizeof(timestamp));
    
    // Level-String
    const char* levelStr = levelToString(level);
    
    // Immer zu Serial ausgeben
    outputToSerial(timestamp, levelStr, tag, message);
    
    // Optional zu SD-Karte schreiben
    if (hasSDCard()) {
        outputToSD(category, timestamp, levelStr, tag, message);
    }
    
    xSemaphoreGive(mutex);
}

void LogHandler::getTimestamp(char* buffer, size_t bufferSize) {
    // Millis-basierter Timestamp (da keine RTC)
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long milliseconds = ms % 1000;
    
    unsigned long hours = (seconds / 3600) % 24;
    unsigned long minutes = (seconds / 60) % 60;
    unsigned long secs = seconds % 60;
    
    snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu.%03lu", 
            hours, minutes, secs, milliseconds);
}

const char* LogHandler::levelToString(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR";
        default:          return "UNKNOWN";
    }
}

const char* LogHandler::categoryToFilename(LogCategory category) {
    switch (category) {
        case LOG_CAT_BATTERY:    return LOGFILE_BATTERY;
        case LOG_CAT_BOOT:       return LOGFILE_BOOT;
        case LOG_CAT_CONNECTION: return LOGFILE_CONNECTION;
        case LOG_CAT_ERROR:      return LOGFILE_ERROR;
        default:                 return LOGFILE_ERROR;
    }
}

void LogHandler::outputToSerial(const char* timestamp, const char* levelStr, 
                                const char* tag, const char* message) {
    Serial.printf("[%s] [%s] [%s] %s\n", timestamp, levelStr, tag, message);
}

void LogHandler::outputToSD(LogCategory category, const char* timestamp, 
                           const char* levelStr, const char* tag, const char* message) {
    const char* filepath = categoryToFilename(category);
    
    // Log-Rotation prüfen
    rotateLogIfNeeded(filepath);
    
    // Log-Zeile formatieren
    char logLine[LOG_MAX_MESSAGE_LEN + 64];
    snprintf(logLine, sizeof(logLine), "[%s] [%s] [%s] %s\n", 
            timestamp, levelStr, tag, message);
    
    // An Datei anhängen
    if (!sdHandler->appendFile(filepath, logLine)) {
        Serial.printf("[LogHandler] Failed to write to %s\n", filepath);
    }
}

void LogHandler::rotateLogIfNeeded(const char* filepath) {
    if (!sdHandler->fileExists(filepath)) {
        return; // Datei existiert noch nicht
    }
    
    size_t fileSize = sdHandler->getFileSize(filepath);
    
    if (fileSize >= LOG_MAX_FILE_SIZE) {
        // Alte rotierte Dateien löschen
        char oldestRotated[64];
        snprintf(oldestRotated, sizeof(oldestRotated), "%s.%d", filepath, LOG_ROTATION_KEEP);
        if (sdHandler->fileExists(oldestRotated)) {
            sdHandler->deleteFile(oldestRotated);
        }
        
        // Dateien rotieren (.2 -> .3, .1 -> .2, etc.)
        for (int i = LOG_ROTATION_KEEP - 1; i >= 1; i--) {
            char oldPath[64], newPath[64];
            snprintf(oldPath, sizeof(oldPath), "%s.%d", filepath, i);
            snprintf(newPath, sizeof(newPath), "%s.%d", filepath, i + 1);
            
            if (sdHandler->fileExists(oldPath)) {
                sdHandler->renameFile(oldPath, newPath);
            }
        }
        
        // Aktuelle Datei zu .1 rotieren
        char rotatedPath[64];
        snprintf(rotatedPath, sizeof(rotatedPath), "%s.1", filepath);
        sdHandler->renameFile(filepath, rotatedPath);
        
        Serial.printf("[LogHandler] Rotated %s (size was %.2f KB)\n", 
                     filepath, fileSize / 1024.0);
    }
}