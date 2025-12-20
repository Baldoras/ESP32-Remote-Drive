/**
 * Globals.h
 * 
 * Globale Deklarationen für ESP32-Remote-Drive
 * Forward Declarations und Extern Declarations
 */

#pragma once

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

class SDCardHandler;
class LogHandler;
class UserConfig;
class SerialCommandHandler;
class PowerManager;
class ESPNowManager;
class BatteryMonitor;

enum class MainCmd : uint8_t;
struct ResultQueueItem;

// ═══════════════════════════════════════════════════════════════════════════
// EXTERN DECLARATIONS - MODUL-INSTANZEN
// ═══════════════════════════════════════════════════════════════════════════

extern SDCardHandler sdCard;
extern LogHandler logger;
extern UserConfig userConfig;
extern SerialCommandHandler serialCmd;
extern PowerManager powerMgr;
extern ESPNowManager espNow;
extern BatteryMonitor battery;

// ═══════════════════════════════════════════════════════════════════════════
// EXTERN DECLARATIONS - GLOBALE VARIABLEN
// ═══════════════════════════════════════════════════════════════════════════

// System-Status
extern bool systemInitialized;
extern unsigned long bootTime;
extern unsigned long lastHeartbeat;

// Motor-Werte (aktuelle Geschwindigkeit)
extern int16_t motorLeftSpeed;   // -100 bis +100
extern int16_t motorRightSpeed;  // -100 bis +100

// Verbindungsstatus
extern bool remoteConnected;
extern unsigned long lastRemoteActivity;

// ═══════════════════════════════════════════════════════════════════════════
// FUNKTIONSDEKLARATIONEN
// ═══════════════════════════════════════════════════════════════════════════

// Initialisierung
bool initializeSystem();
void shutdownSystem();

// Motor-Steuerung
void setMotorSpeed(int16_t left, int16_t right);
void stopMotors();

// LED-Steuerung
void setStatusLED(bool state);
void setErrorLED(bool state);
void blinkStatusLED(int times = 3);

// ESP-NOW Callbacks
void onESPNowDataReceived(const uint8_t* mac, MainCmd cmd, const ResultQueueItem& result);
void onESPNowConnectionChanged(bool connected);

// Telemetrie senden
void sendTelemetry();