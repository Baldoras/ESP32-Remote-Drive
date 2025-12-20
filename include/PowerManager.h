/**
 * PowerManager.h
 * 
 * Power-Management für ESP32-S3 Remote Drive
 * 
 * Features:
 * - Deep-Sleep mit Wake-Up via Timer/GPIO
 * - Auto-Sleep bei kritischer Batterie
 * - Before-Sleep Callback
 * - Wake-Up Reason Detection
 * - LED-Status vor Sleep
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>
#include "setupConf.h"
#include "BatteryMonitor.h"
#include "LogHandler.h"

// Wake-Up Quellen
enum class WakeSource : uint8_t {
    NONE = 0,           // Permanent Power-Off
    TIMER,              // Wake via Timer
    GPIO                // Wake via GPIO (z.B. Button)
};

// Before-Sleep Callback
typedef void (*BeforeSleepCallback)();

class PowerManager {
public:
    /**
     * Konstruktor
     */
    PowerManager();
    
    /**
     * Destruktor
     */
    ~PowerManager();

    /**
     * Power-Manager initialisieren
     * @param logger LogHandler Pointer (optional)
     * @param batteryMon BatteryMonitor Pointer (optional)
     * @return true bei Erfolg
     */
    bool begin(LogHandler* logger = nullptr, BatteryMonitor* batteryMon = nullptr);

    /**
     * Deep-Sleep aktivieren
     * @param wakeSource Wake-Up Quelle
     * @param timerSeconds Timer in Sekunden (nur bei TIMER)
     * @param wakeGpio GPIO-Pin für Wake-Up (nur bei GPIO)
     */
    void sleep(WakeSource wakeSource = WakeSource::TIMER, uint32_t timerSeconds = 0, gpio_num_t wakeGpio = GPIO_NUM_0);

    /**
     * Permanent ausschalten (kein Wake-Up)
     */
    void shutdown();

    /**
     * Soft-Reset (Neustart)
     */
    void restart();

    /**
     * Auto-Sleep bei kritischer Batterie konfigurieren
     * @param enabled Aktiviert?
     * @param wakeSource Wake-Up Quelle für Auto-Sleep
     * @param timerSeconds Timer für Auto-Sleep (optional)
     */
    void setAutoSleepOnCritical(bool enabled, WakeSource wakeSource = WakeSource::TIMER, uint32_t timerSeconds = 0);

    /**
     * Before-Sleep Callback setzen
     * @param callback Funktion die vor Sleep aufgerufen wird
     */
    void setBeforeSleepCallback(BeforeSleepCallback callback);

    /**
     * Wake-Up Grund abrufen (nach Neustart)
     * @return Wake-Up Grund als String
     */
    String getWakeupReason();

    /**
     * Update-Schleife (in loop() aufrufen!)
     * Prüft Auto-Sleep Bedingungen
     */
    void update();

    /**
     * Ist Auto-Sleep aktiv?
     */
    bool isAutoSleepEnabled() const { return autoSleepEnabled; }

private:
    LogHandler* log;
    BatteryMonitor* battery;
    
    bool initialized;
    bool autoSleepEnabled;
    WakeSource autoSleepWakeSource;
    uint32_t autoSleepTimer;
    
    BeforeSleepCallback beforeSleepCallback;
    
    // Critical Battery Warnung
    bool criticalWarningShown;
    unsigned long criticalWarningStart;
    static const uint32_t CRITICAL_WARNING_DURATION = 5000;  // 5 Sekunden Warnung
    
    /**
     * Peripherie herunterfahren
     */
    void shutdownPeripherals();
    
    /**
     * Wake-Up Quellen konfigurieren
     */
    void configureWakeup(WakeSource wakeSource, uint32_t timerSeconds, gpio_num_t wakeGpio);
};

#endif // POWER_MANAGER_H