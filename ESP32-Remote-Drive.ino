/**
 * ESP32-Remote-Drive.ino
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
 * - Verwendet neue ledcAttach() API (statt ledcSetup/ledcAttachPin)
 * - ledcWrite() verwendet Pin direkt (kein Channel mehr)
 */

#include <WiFi.h>
#include <Arduino.h>
#include "include/SDCardHandler.h"
#include "include/LogHandler.h"
#include "include/UserConfig.h"
#include "include/SerialCommandHandler.h"
#include "include/PowerManager.h"
#include "include/ESPNowManager.h"
#include "include/BatteryMonitor.h"
#include "include/setupConf.h"
#include "include/Globals.h"

// Globale Instanzen und Variablen sind in Globals.cpp definiert

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
    
    blinkStatusLED(3);
}

// ═══════════════════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════════════════

void loop() {
    // Serial Commands verarbeiten
    serialCmd.update();
    
    // ESP-NOW Update (prüft Heartbeat/Timeouts, triggert Events)
    espNow.update();
    
    // ESP-NOW Daten verarbeiten (aus Result-Queue)
    ResultQueueItem result;
    while (espNow.getData(&result)) {
        onESPNowDataReceived(result.mac, result.mainCmd, result);
    }
    
    // Batterie-Status prüfen
    battery.update();
    
    // Connection-Timeout prüfen (2 Sekunden)
    if (remoteConnected && (millis() - lastRemoteActivity > 2000)) {
        remoteConnected = false;
        stopMotors();
        setErrorLED(true);
        logger.warning("CONNECTION", "Remote connection timeout");
    }
    
    // Periodisch Telemetrie senden (alle 500ms)
    static unsigned long lastTelemetry = 0;
    if (remoteConnected && (millis() - lastTelemetry > 500)) {
        sendTelemetry();
        lastTelemetry = millis();
    }
    
    // Status-LED blinken wenn verbunden
    static unsigned long lastBlink = 0;
    if (remoteConnected && (millis() - lastBlink > 1000)) {
        setStatusLED(!digitalRead(LED_STATUS));
        lastBlink = millis();
    }
    
    delay(10);
}

// ═══════════════════════════════════════════════════════════════════════════
// SYSTEM-INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════

bool initializeSystem() {
    Serial.println("\n[INIT] Starting system initialization...");
    
    // ─────────────────────────────────────────────────────────────────────
    // LED-Pins initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing LEDs...");
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_ERROR, OUTPUT);
    setStatusLED(false);
    setErrorLED(false);
    
    // ─────────────────────────────────────────────────────────────────────
    // Motor-Pins initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing motors...");
    
    // PWM Setup für linken Motor (ESP32 Core 3.3.0 API)
    ledcAttach(MOTOR_LEFT_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    
    // PWM Setup für rechten Motor (ESP32 Core 3.3.0 API)
    ledcAttach(MOTOR_RIGHT_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    
    // Richtungs-Pins
    pinMode(MOTOR_LEFT_IN1, OUTPUT);
    pinMode(MOTOR_LEFT_IN2, OUTPUT);
    pinMode(MOTOR_RIGHT_IN1, OUTPUT);
    pinMode(MOTOR_RIGHT_IN2, OUTPUT);
    
    // Motoren stoppen
    stopMotors();
    
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
    
    if (sdAvailable && userConfig.isStorageAvailable()) {
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
    battery.setShutdownCallback([](float voltage) {  // float Parameter!
      logger.logf(LOG_ERROR, "BATTERY", 
                "Critical battery %.2fV - shutting down!", voltage);
      Serial.printf("[BATTERY] Critical voltage %.2fV - shutting down!\n", voltage);
      
      stopMotors();
      delay(1000);
      
      powerMgr.shutdown();
    });

    // ─────────────────────────────────────────────────────────────────────
    // Power Manager initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing power manager...");
    powerMgr.begin(&logger, &battery);
    
    // ─────────────────────────────────────────────────────────────────────
    // ESP-NOW initialisieren
    // ─────────────────────────────────────────────────────────────────────
    Serial.println("[INIT] Initializing ESP-NOW...");
    
    if (!espNow.begin(userConfig.getEspnowChannel())) {
        Serial.println("  ❌ ESP-NOW init failed!");
        logger.error("BOOT", "ESP-NOW init failed");
        setErrorLED(true);
        return false;
    }
    
    Serial.println("  ✅ ESP-NOW OK");
    Serial.printf("  MAC: %s\n", espNow.getOwnMacString().c_str());
    
    // Konfiguration anwenden
    espNow.setHeartbeat(true, userConfig.getEspnowHeartbeat());
    espNow.setMaxPeers(userConfig.getEspnowMaxPeers());
    espNow.setTimeout(userConfig.getEspnowTimeout());
    
    Serial.printf("  Heartbeat: %dms, Timeout: %dms\n",
                 userConfig.getEspnowHeartbeat(),
                 userConfig.getEspnowTimeout());
    
    logger.logf(LOG_INFO, "BOOT", "ESP-NOW: MAC=%s", espNow.getOwnMacString().c_str());
    
    // Event-Callbacks für Logging registrieren
    espNow.onEvent(ESPNowEvent::PEER_CONNECTED, [](ESPNowEventData* data) {
        String mac = ESPNowManager::macToString(data->mac);
        logger.logf(LOG_INFO, "CONNECTION", "Peer %s connected", mac.c_str());
        Serial.printf("[ESP-NOW] Peer %s connected\n", mac.c_str());
        setErrorLED(false);
    });
    
    espNow.onEvent(ESPNowEvent::PEER_DISCONNECTED, [](ESPNowEventData* data) {
        String mac = ESPNowManager::macToString(data->mac);
        logger.logf(LOG_WARNING, "CONNECTION", "Peer %s disconnected", mac.c_str());
        Serial.printf("[ESP-NOW] Peer %s disconnected\n", mac.c_str());
        setErrorLED(true);
    });
    
    espNow.onEvent(ESPNowEvent::HEARTBEAT_TIMEOUT, [](ESPNowEventData* data) {
        String mac = ESPNowManager::macToString(data->mac);
        logger.logf(LOG_WARNING, "CONNECTION", "Peer %s timeout", mac.c_str());
        Serial.printf("[ESP-NOW] Peer %s timeout\n", mac.c_str());
        setErrorLED(true);
    });
    
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
    
    stopMotors();
    espNow.end();
    sdCard.end();
    
    Serial.println("[SHUTDOWN] System shutdown complete");
}

// ═══════════════════════════════════════════════════════════════════════════
// MOTOR-STEUERUNG
// ═══════════════════════════════════════════════════════════════════════════

void setMotorSpeed(int16_t left, int16_t right) {
    // Werte begrenzen auf -100 bis +100
    left = constrain(left, -100, 100);
    right = constrain(right, -100, 100);
    
    motorLeftSpeed = left;
    motorRightSpeed = right;
    
    // Linker Motor
    uint8_t leftPWM = abs(left) * 255 / 100;
    if (left > 0) {
        digitalWrite(MOTOR_LEFT_IN1, HIGH);
        digitalWrite(MOTOR_LEFT_IN2, LOW);
    } else if (left < 0) {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, HIGH);
    } else {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, LOW);
    }
    ledcWrite(MOTOR_LEFT_PWM, leftPWM);  // Core 3.3.0: Pin direkt, kein Channel
    
    // Rechter Motor
    uint8_t rightPWM = abs(right) * 255 / 100;
    if (right > 0) {
        digitalWrite(MOTOR_RIGHT_IN1, HIGH);
        digitalWrite(MOTOR_RIGHT_IN2, LOW);
    } else if (right < 0) {
        digitalWrite(MOTOR_RIGHT_IN1, LOW);
        digitalWrite(MOTOR_RIGHT_IN2, HIGH);
    } else {
        digitalWrite(MOTOR_RIGHT_IN1, LOW);
        digitalWrite(MOTOR_RIGHT_IN2, LOW);
    }
    ledcWrite(MOTOR_RIGHT_PWM, rightPWM);  // Core 3.3.0: Pin direkt, kein Channel
}

void stopMotors() {
    setMotorSpeed(0, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// LED-STEUERUNG
// ═══════════════════════════════════════════════════════════════════════════

void setStatusLED(bool state) {
    digitalWrite(LED_STATUS, state ? HIGH : LOW);
}

void setErrorLED(bool state) {
    digitalWrite(LED_ERROR, state ? HIGH : LOW);
}

void blinkStatusLED(int times) {
    for (int i = 0; i < times; i++) {
        setStatusLED(true);
        delay(100);
        setStatusLED(false);
        delay(100);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ESP-NOW CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void onESPNowDataReceived(const uint8_t* mac, MainCmd cmd, const ResultQueueItem& result) {
    lastRemoteActivity = millis();
    
    // Verbindung herstellen
    if (!remoteConnected) {
        remoteConnected = true;
        setErrorLED(false);
        logger.info("CONNECTION", "Remote connected");
    }
    
    // Command verarbeiten
    switch (cmd) {
        case MainCmd::HEARTBEAT:
            lastHeartbeat = millis();
            break;
            
        case MainCmd::DATA_REQUEST:
        case MainCmd::USER_START:
            // Motor-Befehle verarbeiten (aus vorgeparster Struktur)
            if (result.data.hasMotor) {
                int16_t left = result.data.motorLeft;
                int16_t right = result.data.motorRight;
                
                setMotorSpeed(left, right);
                
                logger.logf(LOG_INFO, "MOTOR", "Left: %d, Right: %d", left, right);
            }
            
            // Joystick-Daten verarbeiten (falls vorhanden)
            if (result.data.hasJoystick) {
                // Joystick zu Motor-Geschwindigkeit konvertieren
                int16_t left = result.data.joystickY + result.data.joystickX;  // Vorwärts + Links/Rechts
                int16_t right = result.data.joystickY - result.data.joystickX;
                
                // Auf -100 bis +100 begrenzen
                left = constrain(left, -100, 100);
                right = constrain(right, -100, 100);
                
                setMotorSpeed(left, right);
                
                logger.logf(LOG_INFO, "MOTOR", "Joy: X=%d Y=%d -> L=%d R=%d", 
                           result.data.joystickX, result.data.joystickY, left, right);
            }
            break;
            
        default:
            break;
    }
}

void onESPNowConnectionChanged(bool connected) {
    remoteConnected = connected;
    
    if (!connected) {
        stopMotors();
        setErrorLED(true);
        logger.warning("CONNECTION", "Remote disconnected");
    } else {
        setErrorLED(false);
        logger.info("CONNECTION", "Remote connected");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// TELEMETRIE SENDEN
// ═══════════════════════════════════════════════════════════════════════════

void sendTelemetry() {
    // Paket erstellen
    ESPNowPacket packet(MainCmd::DATA_RESPONSE);
    
    // Batterie-Daten
    uint16_t voltage = (uint16_t)(battery.getVoltage() * 1000); // mV
    uint8_t percent = battery.getPercent();
    packet.addData(DataCmd::BATTERY_VOLTAGE, voltage);
    packet.addData(DataCmd::BATTERY_PERCENT, percent);
    
    // RSSI
    int8_t rssi = WiFi.RSSI();
    packet.addData(DataCmd::RSSI, rssi);
    
    // Verbindungsstatus
    uint8_t connectionStatus = remoteConnected ? 1 : 0;
    packet.addData(DataCmd::CONNECTION, connectionStatus);
    
    // An alle Peers senden (Broadcast)
    espNow.broadcast(packet);
}