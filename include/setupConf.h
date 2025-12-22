/**
 * setupConf.h
 * 
 * Hardware-Konfiguration für ESP32-S3 Remote Drive
 * NICHT ÄNDERN - Hardware-spezifische Einstellungen!
 * 
 * Hardware:
 * - ESP32-S3-N16R8
 * - SD-Karte Reader auf VSPI
 * - Motor-Treiber (L298N/DRV8833)
 * - 4S2P 18650 Li-Ion Akku (mit BMS)
 * - Status-LED
 * 
 * ESP32 Core Version: 3.3.0
 * - PWM Channels werden automatisch verwaltet
 */

#ifndef SETUP_CONF_H
#define SETUP_CONF_H

// ═══════════════════════════════════════════════════════════════════════════
// 💾 SD-KARTE PINS (VSPI)
// ═══════════════════════════════════════════════════════════════════════════

#define SD_CS       38    // SD-Karte Chip Select (LOW = aktiv)
#define SD_MOSI     40    // SD MOSI (VSPI)
#define SD_MISO     41    // SD MISO (VSPI)
#define SD_SCK      39    // SD SCK (VSPI)

// SPI-Geschwindigkeit
#define SD_SPI_FREQUENCY     10000000  // 10 MHz für SD

// ═══════════════════════════════════════════════════════════════════════════
// 🚗 MOTOR-TREIBER PINS (PWM)
// ═══════════════════════════════════════════════════════════════════════════

// Motor Links
#define MOTOR_ENA      10    // PWM für linken Motor (Geschwindigkeit)
#define MOTOR_IN1      11    // IN1 für linken Motor (Richtung)
#define MOTOR_IN2      12    // IN2 für linken Motor (Richtung)

// Motor Rechts
#define MOTOR_ENB     13    // PWM für rechten Motor (Geschwindigkeit)
#define MOTOR_IN3     9     // IN1 für rechten Motor (Richtung)
#define MOTOR_IN4     16    // IN2 für rechten Motor (Richtung)

// Motor PWM Einstellungen
#define MOTOR_PWM_FREQ      20000 // 20 kHz PWM-Frequenz
#define MOTOR_PWM_RES       8     // 8-Bit Auflösung (0-255)

// Motor Identifikation
#define MOTOR_ID_LEFT       0     // Linker Motor
#define MOTOR_ID_RIGHT      1     // Rechter Motor

// ═══════════════════════════════════════════════════════════════════════════
// 💡 STATUS-LED PINS
// ═══════════════════════════════════════════════════════════════════════════

#define LED_STATUS      5     // Status-LED (Connection/Activity)
#define LED_ERROR       6     // Error-LED (Fehler/Warnung)

// ═══════════════════════════════════════════════════════════════════════════
// 🔋 SPANNUNGSSENSOR PIN
// ═══════════════════════════════════════════════════════════════════════════

#define VOLTAGE_SENSOR_PIN  4     // Analog OUT vom Spannungssensor-Modul (GPIO4)

// ═══════════════════════════════════════════════════════════════════════════
// 🔋 BATTERIE SCHWELLWERTE (4S2P 18650 Li-Ion)
// ═══════════════════════════════════════════════════════════════════════════

#define VOLTAGE_RANGE_MAX           25.0  // Modul-Maximum (Hardware-Limit)
#define VOLTAGE_BATTERY_MIN         12.8  // 4S Li-Ion sicher leer (3.2V/Zelle - max. Lebensdauer!)
#define VOLTAGE_BATTERY_MAX         16.8  // 4S Li-Ion voll (4.2V/Zelle)
#define VOLTAGE_BATTERY_NOM         14.8  // 4S Li-Ion nominal (3.7V/Zelle)
#define VOLTAGE_ALARM_LOW           13.2  // Warnung bei <13.2V (3.3V/Zelle)
#define VOLTAGE_SHUTDOWN            12.8  // AUTO-SHUTDOWN bei 12.8V (3.2V/Zelle) ⚠️
#define VOLTAGE_CALIBRATION_FACTOR  0.7   // Kalibrierungsfaktor (Hardware-abhängig!)
#define VOLTAGE_CHECK_INTERVAL      1000  // Spannungs-Check alle 1000ms

// HINWEIS: 
// - Software-Shutdown bei 3.2V/Zelle für maximale Akku-Lebensdauer
// - BMS bietet zusätzlichen Tiefentladungsschutz bei ~2.5V/Zelle
// - Konservative Werte verlängern Lebensdauer erheblich (2000+ Zyklen möglich)

// ═══════════════════════════════════════════════════════════════════════════
// 📝 LOGHANDLER KONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

// Log-Verzeichnis
#define LOG_DIR         "/logs"

// Log-Dateinamen (im /logs Verzeichnis)
#define LOGFILE_BATTERY     "/logs/battery.log"
#define LOGFILE_BOOT        "/logs/boot.log"
#define LOGFILE_CONNECTION  "/logs/connection.log"
#define LOGFILE_ERROR       "/logs/error.log"
#define LOGFILE_MOTOR       "/logs/motor.log"

// Log-Konfiguration
#define LOG_MAX_MESSAGE_LEN 256
#define LOG_MAX_FILE_SIZE   1048576  // 1 MB
#define LOG_ROTATION_KEEP   3        // Anzahl rotierter Dateien

// ═══════════════════════════════════════════════════════════════════════════
// 🛡️ FEHLERCODES
// ═══════════════════════════════════════════════════════════════════════════

enum ErrorCode {
    ERR_NONE = 0,
    ERR_SD_INIT = 3,
    ERR_SD_MOUNT = 4,
    ERR_FILE_OPEN = 5,
    ERR_FILE_WRITE = 6,
    ERR_FILE_READ = 7,
    ERR_BATTERY_INIT = 8,
    ERR_BATTERY_CRITICAL = 9,
    ERR_MOTOR_INIT = 10,
    ERR_ESPNOW_INIT = 11,
    ERR_ESPNOW_PEER = 12
};

// ═══════════════════════════════════════════════════════════════════════════
// 📝 VERSION INFO
// ═══════════════════════════════════════════════════════════════════════════

#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// ═══════════════════════════════════════════════════════════════════════════
// ⚙️ SYSTEM KONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════

#define DEBOUNCE_DELAY      50     // Entprell-Zeit in ms
#define SD_MOUNT_POINT     "/sd"   // Mount-Punkt für SD-Karte
#define SD_MAX_FILES       10      // Maximale Anzahl offener Dateien

// ═══════════════════════════════════════════════════════════════════════════
// 🐛 DEBUG SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

#define SERIAL_BAUD_RATE    115200  // Serielle Baudrate
#define DEBUG_SERIAL        true    // Debug-Ausgaben aktivieren

// Debug-Makros
#if DEBUG_SERIAL
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ═══════════════════════════════════════════════════════════════════════════
// 📡 ESP-NOW HARDWARE-KONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

// Queue-Größen (FreeRTOS)
#ifndef ESPNOW_RX_QUEUE_SIZE
#define ESPNOW_RX_QUEUE_SIZE    10      // Empfangs-Queue Größe
#endif

#ifndef ESPNOW_TX_QUEUE_SIZE
#define ESPNOW_TX_QUEUE_SIZE    10      // Sende-Queue Größe
#endif

#ifndef ESPNOW_RESULT_QUEUE_SIZE
#define ESPNOW_RESULT_QUEUE_SIZE 10     // Ergebnis-Queue für Main-Thread
#endif

// Worker-Task Parameter
#ifndef ESPNOW_WORKER_STACK_SIZE
#define ESPNOW_WORKER_STACK_SIZE 4096   // Worker-Task Stack
#endif

#ifndef ESPNOW_WORKER_PRIORITY
#define ESPNOW_WORKER_PRIORITY   5      // Worker-Task Priorität (höher = wichtiger)
#endif

#ifndef ESPNOW_WORKER_CORE
#define ESPNOW_WORKER_CORE       1      // Core für Worker (0 oder 1, 1 = App-Core)
#endif

// Paket-Größen (ESP-NOW Hardware-Limits)
#ifndef ESPNOW_MAX_PACKET_SIZE
#define ESPNOW_MAX_PACKET_SIZE  250     // ESP-NOW Maximum
#endif

#ifndef ESPNOW_MAX_DATA_SIZE
#define ESPNOW_MAX_DATA_SIZE    248     // Max Nutzdaten (250 - 2 Byte Header)
#endif

// Internes Hardware-Limit für Peers (ESP-NOW Hardware-Beschränkung)
#ifndef ESPNOW_MAX_PEERS_LIMIT
#define ESPNOW_MAX_PEERS_LIMIT  20      // ESP-NOW Hardware-Maximum
#endif

// ═══════════════════════════════════════════════════════════════════════════

#define FIRMWARE_VERSION "1.0.0"
#endif // SETUP_CONF_H