/**
 * SDCardHandler.cpp
 * 
 * Implementation des SD-Karten Handlers (nur File I/O)
 */

#include "include/SDCardHandler.h"

SDCardHandler::SDCardHandler()
    : mounted(false)
    , vspi(nullptr)
    , mutex(nullptr)
{
    mutex = xSemaphoreCreateMutex();
}

SDCardHandler::~SDCardHandler() {
    end();
    
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

bool SDCardHandler::begin() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[SDCard] Failed to acquire mutex!");
        return false;
    }
    
    Serial.println("[SDCard] Initializing SD card...");
    
    // VSPI initialisieren
    vspi = new SPIClass(FSPI);
    vspi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    // SD-Karte mounten
    if (!SD.begin(SD_CS, *vspi, SD_SPI_FREQUENCY)) {
        Serial.println("[SDCard] Mount failed!");
        delete vspi;
        vspi = nullptr;
        xSemaphoreGive(mutex);
        return false;
    }
    
    mounted = true;
    
    // Card-Typ prüfen
    uint8_t cardType = SD.cardType();
    
    if (cardType == CARD_NONE) {
        Serial.println("[SDCard] No SD card detected!");
        end();
        xSemaphoreGive(mutex);
        return false;
    }
    
    Serial.println("[SDCard] SD card mounted successfully");
    Serial.printf("  Type: %s\n", 
                 cardType == CARD_MMC ? "MMC" :
                 cardType == CARD_SD ? "SDSC" :
                 cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    Serial.printf("  Size: %.2f GB\n", getTotalSpace() / 1024.0 / 1024.0 / 1024.0);
    Serial.printf("  Free: %.2f GB\n", getFreeSpace() / 1024.0 / 1024.0 / 1024.0);
    
    xSemaphoreGive(mutex);
    return true;
}

void SDCardHandler::end() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (mounted) {
            flush();
            SD.end();
            mounted = false;
            Serial.println("[SDCard] SD card unmounted");
        }
        
        if (vspi) {
            delete vspi;
            vspi = nullptr;
        }
        
        xSemaphoreGive(mutex);
    }
}

uint64_t SDCardHandler::getFreeSpace() {
    if (!mounted) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t SDCardHandler::getTotalSpace() {
    if (!mounted) return 0;
    return SD.totalBytes();
}

uint64_t SDCardHandler::getUsedSpace() {
    if (!mounted) return 0;
    return SD.usedBytes();
}

// ═══════════════════════════════════════════════════════════════════════════
// FILE OPERATIONEN
// ═══════════════════════════════════════════════════════════════════════════

bool SDCardHandler::writeFile(const char* path, const char* data) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        xSemaphoreGive(mutex);
        return false;
    }
    
    size_t written = file.print(data);
    file.close();
    
    xSemaphoreGive(mutex);
    return written > 0;
}

bool SDCardHandler::appendFile(const char* path, const char* data) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        xSemaphoreGive(mutex);
        return false;
    }
    
    size_t written = file.print(data);
    file.close();
    
    xSemaphoreGive(mutex);
    return written > 0;
}

int SDCardHandler::readFile(const char* path, char* buffer, size_t maxLen) {
    if (!mounted || !buffer) return -1;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return -1;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file) {
        xSemaphoreGive(mutex);
        return -1;
    }
    
    size_t fileSize = file.size();
    size_t readSize = min(fileSize, maxLen - 1);
    
    size_t bytesRead = file.readBytes(buffer, readSize);
    buffer[bytesRead] = '\0';  // Null-Terminierung
    
    file.close();
    xSemaphoreGive(mutex);
    
    return bytesRead;
}

String SDCardHandler::readFileString(const char* path) {
    if (!mounted) return String();
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return String();
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file) {
        xSemaphoreGive(mutex);
        return String();
    }
    
    String content;
    while (file.available()) {
        content += (char)file.read();
    }
    
    file.close();
    xSemaphoreGive(mutex);
    
    return content;
}

bool SDCardHandler::writeBinaryFile(const char* path, const uint8_t* data, size_t len) {
    if (!mounted || !data) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        xSemaphoreGive(mutex);
        return false;
    }
    
    size_t written = file.write(data, len);
    file.close();
    
    xSemaphoreGive(mutex);
    return written == len;
}

int SDCardHandler::readBinaryFile(const char* path, uint8_t* buffer, size_t maxLen) {
    if (!mounted || !buffer) return -1;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return -1;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file) {
        xSemaphoreGive(mutex);
        return -1;
    }
    
    size_t fileSize = file.size();
    size_t readSize = min(fileSize, maxLen);
    
    size_t bytesRead = file.read(buffer, readSize);
    
    file.close();
    xSemaphoreGive(mutex);
    
    return bytesRead;
}

bool SDCardHandler::deleteFile(const char* path) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    bool success = SD.remove(path);
    
    xSemaphoreGive(mutex);
    return success;
}

bool SDCardHandler::fileExists(const char* path) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    bool exists = SD.exists(path);
    
    xSemaphoreGive(mutex);
    return exists;
}

size_t SDCardHandler::getFileSize(const char* path) {
    if (!mounted) return 0;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return 0;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file) {
        xSemaphoreGive(mutex);
        return 0;
    }
    
    size_t size = file.size();
    file.close();
    
    xSemaphoreGive(mutex);
    return size;
}

bool SDCardHandler::renameFile(const char* oldPath, const char* newPath) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    bool success = SD.rename(oldPath, newPath);
    
    xSemaphoreGive(mutex);
    return success;
}

bool SDCardHandler::createDir(const char* path) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    bool success = SD.mkdir(path);
    
    xSemaphoreGive(mutex);
    return success;
}

bool SDCardHandler::removeDir(const char* path) {
    if (!mounted) return false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    
    bool success = SD.rmdir(path);
    
    xSemaphoreGive(mutex);
    return success;
}

void SDCardHandler::listDir(const char* path, void (*callback)(const char* name, bool isDir, size_t size)) {
    if (!mounted || !callback) return;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }
    
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        xSemaphoreGive(mutex);
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        callback(file.name(), file.isDirectory(), file.size());
        file = root.openNextFile();
    }
    
    root.close();
    xSemaphoreGive(mutex);
}

void SDCardHandler::flush() {
    if (!mounted) return;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Nichts zu flushen bei SD-Karte (synchron)
        xSemaphoreGive(mutex);
    }
}

void SDCardHandler::printInfo() {
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.println("SDCardHandler Info:");
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.printf("  Mounted: %s\n", mounted ? "Yes" : "No");
    
    if (mounted) {
        uint8_t cardType = SD.cardType();
        Serial.printf("  Card Type: %s\n", 
                     cardType == CARD_MMC ? "MMC" :
                     cardType == CARD_SD ? "SDSC" :
                     cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
        Serial.printf("  Total Space: %.2f GB\n", getTotalSpace() / 1024.0 / 1024.0 / 1024.0);
        Serial.printf("  Used Space: %.2f GB\n", getUsedSpace() / 1024.0 / 1024.0 / 1024.0);
        Serial.printf("  Free Space: %.2f GB\n", getFreeSpace() / 1024.0 / 1024.0 / 1024.0);
    }
    
    Serial.println("═══════════════════════════════════════════════════════");
}