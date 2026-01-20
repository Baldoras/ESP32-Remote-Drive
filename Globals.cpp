/**
 * Globals.cpp
 * 
 * Globale Instanzen für ESP32-Remote-Drive
 */

// ═══════════════════════════════════════════════════════════════════════════
// INCLUDES
// ═══════════════════════════════════════════════════════════════════════════

#include "include/Globals.h"
#include "include/setupConf.h"
#include "include/userConf.h"
#include "include/SDCardHandler.h"
#include "include/LogHandler.h"
#include "include/UserConfig.h"
#include "include/SerialCommandHandler.h"
#include "include/PowerManager.h"
#include "include/ESPNowRemoteController.h"
#include "include/BatteryMonitor.h"

// ═══════════════════════════════════════════════════════════════════════════
// GLOBALE MODUL-INSTANZEN
// ═══════════════════════════════════════════════════════════════════════════

SDCardHandler sdCard;
LogHandler logger(nullptr, LOG_INFO);  // Startet ohne SD, nur Serial, Level INFO
UserConfig userConfig;
SerialCommandHandler serialCmd;
PowerManager powerMgr;
ESPNowRemoteController espNow;
BatteryMonitor battery;

// ═══════════════════════════════════════════════════════════════════════════
// GLOBALE VARIABLEN
// ═══════════════════════════════════════════════════════════════════════════

bool systemInitialized = false;
unsigned long bootTime = 0;
unsigned long lastHeartbeat = 0;

int16_t motorLeftSpeed = 0;
int16_t motorRightSpeed = 0;

bool remoteConnected = false;
unsigned long lastRemoteActivity = 0;