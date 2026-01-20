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
// 📡 ESP-NOW BENUTZER-EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define ESPNOW_MAX_PEERS          1                   // Maximale Anzahl Peers
#define ESPNOW_CHANNEL            2                   // WiFi-Kanal (0 = auto)
#define ESPNOW_HEARTBEAT_INTERVAL 500                 // Heartbeat alle 500ms
#define ESPNOW_TIMEOUT            30000                // Verbindungs-Timeout 2s
#define ESPNOW_PEER_MAC           "10:20:BA:4D:6C:E4" // Peer device MAC (Beispiel)

// ═══════════════════════════════════════════════════════════════════════════
// 🔧 DEBUG EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define AUTO_SHUTDOWN        false    // Auto shutdown enabled
// ═══════════════════════════════════════════════════════════════════════════
// 🔧 DEBUG EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════════════════

#define DEBUG_SERIAL         true    // Debug-Ausgaben aktivieren


#endif // USER_CONF_H