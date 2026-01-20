/**
 * ESP32-Remote-Drive_ino.cpp
 * 
 * Remote-Gegenstelle (Drive) für ESP32-Remote-UI
 * Empfängt Befehle via ESP-NOW und steuert Motoren
 * Sendet Telemetriedaten zurück
 * 
 * Hardware:
 * - ESP32-S3-N16R8
 * - L298N/DRV8833 Motor-Treiber
 * - 4S2P 18650 Li-Ion Akku (mit BMS)
 * - SD-Karte für Logging
 * 
 * ESP32 Core Version: 3.3.0
 */

#include <WiFi.h>
#include <Arduino.h>
#include "include/SDCardHandler.h"
#include "include/LogHandler.h"
#include "include/UserConfig.h"
#include "include/SerialCommandHandler.h"
#include "include/PowerManager.h"
#include "include/ESPNowRemoteController.h"
#include "include/BatteryMonitor.h"
#include "include/MotorController.h"
#include "include/setupConf.h"
#include "include/Globals.h"

// Globale Instanzen und Variablen sind in Globals.cpp definiert

// Motor Controller Instanz
MotorController motorCtrl;

// ═══════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
    // Serial initialisieren
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.println("ESP32-Remote-Drive Starting...");
    Serial.println("Version: " FIRMWARE_VERSION);
    Serial.println("Build: " BUILD_DATE " " BUILD_TIME);
    Serial.println("═══════════════════════════════════════════════════════");
    
    // System initialisieren
    if (!initializeSystem()) {
        Serial.println("[ERROR] System initialization failed!");
        Serial.println("System will restart in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
    
    bootTime = millis();
    systemInitialized = true;
    
    Serial.println("═══════════════════════════════════════════════════════");
    Serial.println("System Ready!");
    Serial.println("═══════════════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════════════════

void loop() {
    // Serial Commands verarbeiten
    serialCmd.update();
    
    // ESP-NOW Update (prüft Heartbeat/Timeouts, verarbeitet RX-Queue)
    espNow.update();
    
    // Batterie-Status prüfen
    battery.update();
    
    // Motor Controller Update
    motorCtrl.update();
    
    // Connection-Timeout prüfen (2 Sekunden)
    if (remoteConnected && (millis() - lastRemoteActivity > 2000)) {
        remoteConnected = false;
        motorCtrl.stop();
        logger.warning("CONNECTION", "Remote connection timeout");
    }
    
    // Periodisch Telemetrie senden (alle 500ms)
    /*static unsigned long lastTelemetry = 0;
    if (remoteConnected && (millis() - lastTelemetry > 500)) {
        sendTelemetry();
        lastTelemetry = millis();
    }*/
    
    delay(10);
}

// ═══════════════════════════════════════════════════════════════════════════
// SYSTEM-INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════

bool initializeSystem() {
    Serial.println("\n[INIT] Starting system initialization...");
    
    // ─────────────────────────────────────────────────────────────────────
    // SD-Card initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing SD card...");
    bool sdAvailable = sdCard.begin();
    
    if (sdAvailable) {
        Serial.println("  ✅ SD-Card OK");
        logger.setSDHandler(&sdCard);
        logger.info("BOOT", "System starting...");
        logger.info("BOOT", "Version: " FIRMWARE_VERSION);
    } else {
        Serial.println("  ⚠️ SD-Card N/A (logging to Serial only)");
        logger.info("BOOT", "System starting (no SD card)");
        logger.info("BOOT", "Version: " FIRMWARE_VERSION);
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // Config laden
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Loading configuration...");
    userConfig.init("/config.json", &sdCard);
    
    if (sdAvailable) {
        if (userConfig.load()) {
            Serial.println("  ✅ Config loaded from SD");
            logger.info("BOOT", "Config loaded successfully");
        } else {
            Serial.println("  ⚠️ Config not found, using defaults");
            logger.warning("BOOT", "Config not found, using defaults");
            userConfig.reset();
        }
    } else {
        Serial.println("  ℹ️ Using default config (no SD card)");
        logger.info("BOOT", "Using default config");
        userConfig.reset();
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // WiFi initialisieren (für ESP-NOW erforderlich!)
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing WiFi for ESP-NOW...");
    
    // WiFi in Station-Modus setzen
    WiFi.mode(WIFI_STA);
    
    // WiFi-Verbindung trennen (keine AP-Verbindung nötig)
    WiFi.disconnect();
    
    // Optional: WiFi-Kanal setzen (falls in Config angegeben)
    uint8_t wifiChannel = userConfig.getEspnowChannel();
    if (wifiChannel > 0 && wifiChannel <= 14) {
        esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE);
        Serial.printf("  WiFi Channel: %d (from config)\n", wifiChannel);
    } else {
        Serial.println("  WiFi Channel: Auto (0 in config)");
    }
    
    Serial.println("  ✅ WiFi ready for ESP-NOW");
    Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("  Mode: %s\n", WiFi.getMode() == WIFI_STA ? "STA" : "OTHER");
    Serial.printf("  Channel: %d\n", WiFi.channel());
    
    // ─────────────────────────────────────────────────────────────────────
    // Battery Monitor initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing battery monitor...");
    if (battery.begin()) {
        Serial.println("  ✅ Battery OK");
        Serial.printf("  Voltage: %.2fV, Percent: %d%%\n", 
                     battery.getVoltage(), battery.getPercent());
        logger.logf(LOG_INFO, "BOOT", "Battery: %.2fV (%d%%)", 
                   battery.getVoltage(), battery.getPercent());
    } else {
        Serial.println("  ⚠️ Battery sensor error");
        logger.error("BOOT", "Battery init failed", ERR_BATTERY_INIT);
    }
    
    // Callback für kritische Batterie
    battery.setShutdownCallback([](float voltage) {
        logger.logf(LOG_ERROR, "BATTERY", "Critical battery %.2fV - shutting down!", voltage);
        Serial.printf("[BATTERY] Critical voltage %.2fV - shutting down!\n", voltage);
        
        motorCtrl.stop();
        delay(1000);
        
        powerMgr.shutdown();
    });
    
    // ─────────────────────────────────────────────────────────────────────
    // Power Manager initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing power manager...");
    powerMgr.begin(&logger, &battery);
    
    // ─────────────────────────────────────────────────────────────────────
    // Motor Controller initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing motor controller...");
    motorCtrl.begin();
    motorCtrl.enable();
    Serial.println("  ✅ Motor Controller OK");
    
    // ─────────────────────────────────────────────────────────────────────
    // ESP-NOW Remote Controller initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing ESP-NOW Remote Controller...");
    
    if (!espNow.begin(userConfig.getEspnowChannel())) {
        Serial.println("  ❌ ESP-NOW init failed!");
        logger.error("BOOT", "ESP-NOW init failed");
        return false;
    }
    
    Serial.println("  ✅ ESP-NOW Remote Controller OK");
    Serial.printf("  MAC: %s\n", espNow.getOwnMacString().c_str());
    Serial.printf("  WiFi Channel: %d\n", WiFi.channel());
    Serial.printf("  Expected Master MAC: %s\n", userConfig.getEspnowPeerMac());
    
    // Konfiguration anwenden
    espNow.setHeartbeat(true, userConfig.getEspnowHeartbeat());
    espNow.setMaxPeers(userConfig.getEspnowMaxPeers());
    espNow.setTimeout(userConfig.getEspnowTimeout());
    
    Serial.printf("  Heartbeat: %dms, Timeout: %dms\n",
                 userConfig.getEspnowHeartbeat(),
                 userConfig.getEspnowTimeout());
    
    logger.logf(LOG_INFO, "BOOT", "ESP-NOW: MAC=%s", espNow.getOwnMacString().c_str());
    
    // ─────────────────────────────────────────────────────────────────────
    // ESP-NOW Callbacks registrieren
    // ─────────────────────────────────────────────────────────────────────
    
    // Event-Callbacks für Logging
    espNow.onEvent(ESPNowEvent::PEER_CONNECTED, [](ESPNowEventData* data) {
        String mac = ESPNowRemoteController::macToString(data->mac);
        logger.logConnection(mac.c_str(), "connected");
        Serial.printf("[ESP-NOW] ✅ Peer %s connected\n", mac.c_str());
        
        remoteConnected = true;
        lastRemoteActivity = millis();
    });
    
    espNow.onEvent(ESPNowEvent::PEER_DISCONNECTED, [](ESPNowEventData* data) {
        String mac = ESPNowRemoteController::macToString(data->mac);
        logger.logConnection(mac.c_str(), "disconnected");
        Serial.printf("[ESP-NOW] ❌ Peer %s disconnected\n", mac.c_str());
        
        remoteConnected = false;
        motorCtrl.stop();
    });
    
    espNow.onEvent(ESPNowEvent::HEARTBEAT_TIMEOUT, [](ESPNowEventData* data) {
        String mac = ESPNowRemoteController::macToString(data->mac);
        logger.logConnection(mac.c_str(), "timeout");
        Serial.printf("[ESP-NOW] ⚠️ Peer %s timeout\n", mac.c_str());
    });
    
    // DATA_RECEIVED Event für Debug
    espNow.onEvent(ESPNowEvent::DATA_RECEIVED, [](ESPNowEventData* data) {
        String mac = ESPNowRemoteController::macToString(data->mac);
        Serial.printf("[ESP-NOW] DATA_RECEIVED from %s\n", mac.c_str());
    });
    
    // Joystick-Callback (High-Level)
    /*espNow.setJoystickCallback([](const uint8_t* mac, const JoystickData& data) {
        // Joystick-Daten an MotorController weiterleiten
        // Skalierung: JoystickData ist -32768 bis +32767, wir brauchen -100 bis +100
        motorCtrl.processMovementInput((int8_t)(data.x / 327), (int8_t)(data.y / 327));
        
        lastRemoteActivity = millis();
        
        // Debug-Ausgabe (max 1x pro Sekunde)
        static unsigned long lastJoyDebug = 0;
        if (millis() - lastJoyDebug >= 1000) {
            Serial.printf("[JOYSTICK] X=%d Y=%d Btn=%d\n", data.x, data.y, data.button);
            lastJoyDebug = millis();
        }
        
        logger.logf(LOG_DEBUG, "MOTOR", "Joystick: X=%d Y=%d Btn=%d", 
                   data.x, data.y, data.button);
    });*/
    
    // ─────────────────────────────────────────────────────────────────────
    // Serial Command Handler initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing serial command handler...");
    serialCmd.begin(&sdCard, &logger, &battery, &espNow, &userConfig);
    
    Serial.println("[INIT] System initialization complete!");
    logger.info("BOOT", "System ready");
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// SHUTDOWN
// ═══════════════════════════════════════════════════════════════════════════

void shutdownSystem() {
    logger.info("BOOT", "System shutting down");
    
    motorCtrl.stop();
    espNow.end();
    sdCard.end();
    
    Serial.println("[SHUTDOWN] System shutdown complete");
}

// ═══════════════════════════════════════════════════════════════════════════
// TELEMETRIE SENDEN
// ═══════════════════════════════════════════════════════════════════════════

/*void sendTelemetry() {
    // Telemetrie-Daten zusammenstellen
    TelemetryData telemetry;
    telemetry.batteryVoltage = (uint16_t)(battery.getVoltage() * 1000); // mV
    telemetry.batteryPercent = battery.getPercent();
    telemetry.temperature = 0;  // TODO: Temperature sensor
    telemetry.rssi = WiFi.RSSI();
    
    // Motor-Telemetrie als RemoteESPNowPacket
    RemoteESPNowPacket packet;
    packet.begin(MainCmd::DATA_RESPONSE);
    packet.addTelemetry(telemetry);
    
    // Motor-Status
    MotorTelemetry motorTel = motorCtrl.getTelemetry();
    packet.addMotors(motorTel.leftSpeed, motorTel.rightSpeed);
    
    // Verbindungsstatus
    uint8_t connectionStatus = remoteConnected ? 1 : 0;
    packet.add(static_cast<DataCmd>(static_cast<uint8_t>(RemoteDataCmd::CONNECTION)), 
               &connectionStatus, 1);
    
    // An alle Peers senden (Broadcast)
    espNow.broadcast(packet);
}*/