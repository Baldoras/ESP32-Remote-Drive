/**
 * PowerManager.cpp
 * 
 * Implementation des Power-Managers für Remote Drive
 */

#include "include/PowerManager.h"

PowerManager::PowerManager()
    : log(nullptr)
    , battery(nullptr)
    , initialized(false)
    , autoSleepEnabled(false)
    , autoSleepWakeSource(WakeSource::TIMER)
    , autoSleepTimer(0)
    , beforeSleepCallback(nullptr)
    , criticalWarningShown(false)
    , criticalWarningStart(0)
{
}

PowerManager::~PowerManager() {
}

bool PowerManager::begin(LogHandler* logger, BatteryMonitor* batteryMon) {
    log = logger;
    battery = batteryMon;
    
    DEBUG_PRINTLN("[PowerManager] Initialisiere...");
    
    // Wake-Up Grund ausgeben
    String wakeupReason = getWakeupReason();
    DEBUG_PRINTF("[PowerManager] Wake-Up Grund: %s\n", wakeupReason.c_str());
    
    if (log) {
        log->logf(LogLevel::LOG_INFO, "POWER", "PowerManager initialized");
        log->logf(LogLevel::LOG_INFO, "POWER", (String("Wakeup: ") + wakeupReason).c_str());
    }
    
    initialized = true;
    
    DEBUG_PRINTLN("[PowerManager] ✅ Initialisiert");
    
    return true;
}

void PowerManager::sleep(WakeSource wakeSource, uint32_t timerSeconds, gpio_num_t wakeGpio) {
    if (!initialized) {
        DEBUG_PRINTLN("[PowerManager] ❌ Nicht initialisiert!");
        return;
    }
    
    DEBUG_PRINTLN("\n╔════════════════════════════════════════╗");
    DEBUG_PRINTLN("║       ENTERING SLEEP MODE              ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════╝");
    
    if (log) {
        log->logf(LogLevel::LOG_INFO, "POWER", "Entering sleep mode");
    }
    
    // Before-Sleep Callback
    if (beforeSleepCallback != nullptr) {
        DEBUG_PRINTLN("[PowerManager] Führe Before-Sleep Callback aus...");
        beforeSleepCallback();
    }
    
    // Peripherie herunterfahren
    DEBUG_PRINTLN("[PowerManager] Shutdown Peripherals...");
    shutdownPeripherals();
    
    // Wake-Up Quellen konfigurieren
    DEBUG_PRINTLN("[PowerManager] Konfiguriere Wake-Up...");
    configureWakeup(wakeSource, timerSeconds, wakeGpio);
    
    DEBUG_PRINTLN("[PowerManager] ✅ Entering Deep-Sleep NOW!");
    delay(100);  // Zeit für letzte Serial-Ausgabe
    
    // Deep-Sleep!
    esp_deep_sleep_start();
}

void PowerManager::shutdown() {
    DEBUG_PRINTLN("[PowerManager] ⚠️ PERMANENT POWER-OFF!");
    
    if (log) {
        log->logf(LogLevel::LOG_WARNING, "POWER", "Permanent shutdown");
    }
    
    sleep(WakeSource::NONE, 0);
}

void PowerManager::restart() {
    DEBUG_PRINTLN("[PowerManager] 🔄 RESTART!");
    
    if (log) {
        log->logf(LogLevel::LOG_INFO, "POWER", "System restart");
    }
    
    // Before-Sleep Callback
    if (beforeSleepCallback != nullptr) {
        beforeSleepCallback();
    }
    
    delay(500);
    ESP.restart();
}

void PowerManager::setAutoSleepOnCritical(bool enabled, WakeSource wakeSource, uint32_t timerSeconds) {
    autoSleepEnabled = enabled;
    autoSleepWakeSource = wakeSource;
    autoSleepTimer = timerSeconds;
    
    DEBUG_PRINTF("[PowerManager] Auto-Sleep bei Critical Battery: %s\n", 
                 enabled ? "AKTIVIERT" : "DEAKTIVIERT");
    
    if (log) {
        log->logf(LogLevel::LOG_INFO, "POWER", (String("Auto-Sleep: ") + (enabled ? "enabled" : "disabled")).c_str());
    }
    
    if (enabled) {
        DEBUG_PRINTF("  Wake-Source: %d, Timer: %lus\n", 
                     static_cast<uint8_t>(wakeSource), timerSeconds);
        
        // BatteryMonitor Auto-Shutdown deaktivieren (Konflikt vermeiden)
        if (battery) {
            battery->setAutoShutdown(false);
            DEBUG_PRINTLN("  BatteryMonitor Auto-Shutdown deaktiviert (PowerManager übernimmt)");
            
            if (log) {
                log->logf(LogLevel::LOG_INFO, "POWER", "BatteryMonitor auto-shutdown disabled");
            }
        }
    }
}

void PowerManager::setBeforeSleepCallback(BeforeSleepCallback callback) {
    beforeSleepCallback = callback;
}

String PowerManager::getWakeupReason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            return "EXT0 (GPIO)";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "EXT1 (Multiple GPIOs)";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "Touchpad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "GPIO";
        case ESP_SLEEP_WAKEUP_UART:
            return "UART";
        case ESP_SLEEP_WAKEUP_WIFI:
            return "WiFi";
        case ESP_SLEEP_WAKEUP_COCPU:
            return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            return "COCPU Trap";
        case ESP_SLEEP_WAKEUP_BT:
            return "Bluetooth";
        default:
            return "Power-On / Reset";
    }
}

void PowerManager::update() {
    if (!initialized || !autoSleepEnabled || !battery) {
        return;
    }
    
    // Prüfen ob Batterie kritisch
    if (battery->isCritical()) {
        if (!criticalWarningShown) {
            // Erste Warnung
            DEBUG_PRINTLN("\n⚠️⚠️⚠️ CRITICAL BATTERY - AUTO-SLEEP IN 5s! ⚠️⚠️⚠️");
            DEBUG_PRINTF("Spannung: %.2fV\n", battery->getVoltage());
            
            if (log) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Critical battery: %.2fV - shutdown in 5s", 
                        battery->getVoltage());
                log->logf(LogLevel::LOG_ERROR, "POWER", msg);
            }
            
            /*// LED-Blink-Pattern für Warnung
            for (int i = 0; i < 5; i++) {
                digitalWrite(LED_ERROR, HIGH);
                delay(100);
                digitalWrite(LED_ERROR, LOW);
                delay(100);
            }*/
            
            criticalWarningShown = true;
            criticalWarningStart = millis();
        } else {
            // Prüfen ob Warnung-Zeit abgelaufen
            if (millis() - criticalWarningStart >= CRITICAL_WARNING_DURATION) {
                DEBUG_PRINTLN("[PowerManager] Auto-Sleep wird ausgelöst!");
                
                if (log) {
                    log->logf(LogLevel::LOG_WARNING, "POWER", "Auto-sleep triggered");
                }
                
                sleep(autoSleepWakeSource, autoSleepTimer);
            }
        }
    } else {
        // Batterie wieder OK → Warnung zurücksetzen
        if (criticalWarningShown) {
            criticalWarningShown = false;
            DEBUG_PRINTLN("[PowerManager] Critical Battery Warnung zurückgesetzt");
            
            if (log) {
                log->logf(LogLevel::LOG_INFO, "POWER", "Battery recovered from critical");
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE METHODEN
// ═══════════════════════════════════════════════════════════════════════════

void PowerManager::shutdownPeripherals() {
    // LEDs ausschalten
    //digitalWrite(LED_STATUS, LOW);
    //digitalWrite(LED_ERROR, LOW);
    
    DEBUG_PRINTLN("  LEDs ausgeschaltet");
    DEBUG_PRINTLN("  Peripherals shutdown complete");
}

void PowerManager::configureWakeup(WakeSource wakeSource, uint32_t timerSeconds, gpio_num_t wakeGpio) {
    // ALLE Wake-Up Quellen zurücksetzen
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    switch (wakeSource) {
        case WakeSource::NONE:
            DEBUG_PRINTLN("  Wake-Up: NONE (Permanent Off)");
            if (log) {
                log->logf(LogLevel::LOG_WARNING, "POWER", "Wake-up: NONE (permanent off)");
            }
            // Keine Wake-Up Quellen → Permanent aus
            break;
            
        case WakeSource::TIMER:
            DEBUG_PRINTF("  Wake-Up: Timer (%lu seconds)\n", timerSeconds);
            if (timerSeconds > 0) {
                esp_sleep_enable_timer_wakeup(timerSeconds * 1000000ULL);  // Mikrosekunden
                
                if (log) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Wake-up: Timer %lus", timerSeconds);
                    log->logf(LogLevel::LOG_INFO, "POWER", msg);
                }
            }
            break;
            
        case WakeSource::GPIO:
            DEBUG_PRINTF("  Wake-Up: GPIO %d (LOW trigger)\n", wakeGpio);
            // EXT0: Single GPIO, LOW-Trigger
            esp_sleep_enable_ext0_wakeup(wakeGpio, 0);  // 0 = LOW
            
            if (log) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Wake-up: GPIO %d", wakeGpio);
                log->logf(LogLevel::LOG_INFO, "POWER", msg);
            }
            break;
    }
}