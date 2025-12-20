/**
 * userConf.h
 * 
 * Benutzer-Konfiguration für ESP32-S3 Remote Control
 * Diese Werte können über SD-Card (config.json) oder UI überschrieben werden
 * 
 * Default-Werte für:
 * - Backlight-Helligkeit
 * - Touch-Kalibrierung
 * - ESP-NOW Parameter
 * - Joystick Parameter
 * - Debug-Einstellungen
 */

#ifndef USER_CONF_H
#define USER_CONF_H

// ═══════════════════════════════════════════════════════════════════════════
// ⚡ BACKLIGHT BENUTZER-EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define BACKLIGHT_DEFAULT   20     // Standard-Helligkeit (0-255)

// ═══════════════════════════════════════════════════════════════════════════
// 👆 TOUCH KALIBRIERUNG (Benutzer-anpassbar)
// ═══════════════════════════════════════════════════════════════════════════

#define TOUCH_MIN_X      300    // Minimum X (nach erstem Start anpassen!)
#define TOUCH_MAX_X      3800   // Maximum X
#define TOUCH_MIN_Y      300    // Minimum Y
#define TOUCH_MAX_Y      3800   // Maximum Y
#define TOUCH_ROTATION   1      // Touch-Rotation (0-3)
#define TOUCH_THRESHOLD  40     // Mindestdruck für Touch-Erkennung

// ═══════════════════════════════════════════════════════════════════════════
// 📡 ESP-NOW BENUTZER-EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define ESPNOW_MAX_PEERS          5                   // Maximale Anzahl Peers
#define ESPNOW_CHANNEL            0                   // WiFi-Kanal (0 = auto)
#define ESPNOW_HEARTBEAT_INTERVAL 500                 // Heartbeat alle 500ms
#define ESPNOW_TIMEOUT_MS         2000                // Verbindungs-Timeout 2s
#define ESPNOW_PEER_DEVICE_MAC    "11:22:33:44:55:66" // Peer device MAC (Beispiel)

// ═══════════════════════════════════════════════════════════════════════════
// 🕹️ JOYSTICK BENUTZER-EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define JOY_DEADZONE         5     // Deadzone in % (0-100)
#define JOY_UPDATE_INTERVAL  20    // Update-Intervall in ms
#define JOY_INVERT_X         true  // X-Achse invertieren
#define JOY_INVERT_Y         true  // Y-Achse invertieren

// Kalibrierung (12-Bit ADC: 0-4095)
#define JOY_CAL_X_MIN        100   // X-Achse Minimum
#define JOY_CAL_X_CENTER     2048  // X-Achse Center
#define JOY_CAL_X_MAX        4000  // X-Achse Maximum

#define JOY_CAL_Y_MIN        100   // Y-Achse Minimum
#define JOY_CAL_Y_CENTER     2048  // Y-Achse Center
#define JOY_CAL_Y_MAX        4000  // Y-Achse Maximum

// ═══════════════════════════════════════════════════════════════════════════
// 🔧 DEBUG EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define AUTO_SHUTDOWN        true    // Auto shutdown enabled
// ═══════════════════════════════════════════════════════════════════════════
// 🔧 DEBUG EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define DEBUG_SERIAL         true    // Debug-Ausgaben aktivieren


#endif // USER_CONF_H