/**
 * BatteryMonitor.h
 * 
 * Batterie-Überwachung für 2S LiPo (6.6V - 8.4V)
 * 
 * Features:
 * - Spannungsmessung mit Glättung (Moving Average)
 * - Prozentberechnung (0-100%)
 * - Low-Voltage Warnung
 * - Auto-Shutdown bei Unterspannung
 * - Callback-Funktionen für Events
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "SDCardHandler.h"
#include "Globals.h"
#include <Arduino.h>
#include "setupConf.h"

// Callback-Typen
typedef void (*BatteryWarningCallback)(float voltage, uint8_t percent);
typedef void (*BatteryShutdownCallback)(float voltage);

class BatteryMonitor {
public:
    /**
     * Konstruktor
     */
    BatteryMonitor();

    /**
     * Destruktor
     */
    ~BatteryMonitor();

    /**
     * Batteriemonitor initialisieren
     * @return true bei Erfolg
     */
    bool begin();

    /**
     * Spannungsmessung aktualisieren (in loop() aufrufen!)
     * @return true wenn neue Messung durchgeführt wurde
     */
    bool update();

    /**
     * Aktuelle Batteriespannung abrufen (gefiltert)
     * @return Spannung in Volt
     */
    float getVoltage();

    /**
     * Rohe Batteriespannung abrufen (ungefiltert)
     * @return Spannung in Volt
     */
    float getRawVoltage();

    /**
     * Batterie-Ladezustand abrufen
     * @return Prozent (0-100)
     */
    uint8_t getPercent();

    /**
     * Ist Batterie in kritischem Zustand?
     * @return true wenn Spannung <= VOLTAGE_SHUTDOWN
     */
    bool isCritical();

    /**
     * Ist Low-Battery Warnung aktiv?
     * @return true wenn Spannung <= VOLTAGE_ALARM_LOW
     */
    bool isLow();

    /**
     * Callback für Low-Voltage Warnung setzen
     * @param callback Funktion die bei Warnung aufgerufen wird
     */
    void setWarningCallback(BatteryWarningCallback callback);

    /**
     * Callback für Shutdown setzen
     * @param callback Funktion die vor Shutdown aufgerufen wird
     */
    void setShutdownCallback(BatteryShutdownCallback callback);

    /**
     * Auto-Shutdown aktivieren/deaktivieren
     * @param enabled true = aktiviert, false = deaktiviert
     */
    void setAutoShutdown(bool enabled);

    /**
     * Shutdown manuell auslösen
     */
    void shutdown();

    /**
     * Debug-Informationen ausgeben
     */
    void printInfo();

private:
    bool initialized;              // Initialisierungs-Flag
    bool autoShutdownEnabled;      // Auto-Shutdown aktiv?
    
    // Spannungsmessung
    float currentVoltage;          // Aktuelle gefilterte Spannung
    float rawVoltage;              // Rohe ungefilterte Spannung
    uint8_t currentPercent;        // Aktueller Ladezustand in %
    
    // Moving Average Filter
    static const uint8_t FILTER_SAMPLES = 10;
    float voltageBuffer[FILTER_SAMPLES];
    uint8_t bufferIndex;
    bool bufferFilled;
    
    // Timing
    unsigned long lastUpdateTime;
    unsigned long lastWarningTime;
    
    // Status-Flags
    bool warningActive;            // Warnung wurde ausgegeben
    bool criticalActive;           // Kritischer Zustand aktiv
    
    // Callbacks
    BatteryWarningCallback warningCallback;
    BatteryShutdownCallback shutdownCallback;
    
    /**
     * Rohe ADC-Spannung auslesen
     * @return Spannung in Volt
     */
    float readRawVoltage();
    
    /**
     * Spannung filtern (Moving Average)
     * @param newVoltage Neue Messung
     * @return Gefilterte Spannung
     */
    float filterVoltage(float newVoltage);
    
    /**
     * Spannung in Prozent umrechnen
     * @param voltage Spannung in Volt
     * @return Prozent (0-100)
     */
    uint8_t voltageToPercent(float voltage);
    
    /**
     * Warnungen prüfen und ausgeben
     */
    void checkWarnings();
    
    /**
     * Shutdown-Bedingung prüfen
     */
    void checkShutdown();
};

#endif // BATTERY_MONITOR_H