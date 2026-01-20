/**
 * BatteryMonitor.h
 * 
 * Batterie-Überwachung für 4S Li-Ion (12.8V - 16.8V) mit ACS712-20A Stromsensor
 * 
 * Features:
 * - Spannungsmessung mit Glättung (Moving Average)
 * - Strommessung mit ACS712-20A (3.3V Versorgung)
 * - Leistungsberechnung (Watt)
 * - Energieverbrauch (mAh, Wh)
 * - Prozentberechnung (0-100%)
 * - Low-Voltage Warnung
 * - High-Current Warnung
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
typedef void (*CurrentWarningCallback)(float current, float power);

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
     * Spannungs- und Strommessung aktualisieren (in loop() aufrufen!)
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
     * Aktuellen Strom abrufen (gefiltert)
     * @return Strom in Ampere
     */
    float getCurrent();

    /**
     * Roher Strom abrufen (ungefiltert)
     * @return Strom in Ampere
     */
    float getRawCurrent();

    /**
     * Aktuelle Leistung abrufen
     * @return Leistung in Watt
     */
    float getPower();

    /**
     * Verbrauchte Energie abrufen
     * @return Energie in mAh
     */
    float getConsumedMAh();

    /**
     * Verbrauchte Energie abrufen
     * @return Energie in Wh
     */
    float getConsumedWh();

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
     * Ist Strom über Warnlimit?
     * @return true wenn Strom >= CURRENT_WARNING
     */
    bool isCurrentHigh();

    /**
     * Stromsensor kalibrieren (Nullpunkt-Offset)
     * Sollte ohne Last aufgerufen werden
     * @param samples Anzahl Messungen für Durchschnitt (default: 100)
     */
    void calibrateCurrent(uint16_t samples = 100);

    /**
     * Energiezähler zurücksetzen
     */
    void resetEnergyCounters();

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
     * Callback für High-Current Warnung setzen
     * @param callback Funktion die bei hohem Strom aufgerufen wird
     */
    void setCurrentWarningCallback(CurrentWarningCallback callback);

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
    
    // Strommessung
    float currentCurrent;          // Aktueller gefilterter Strom (A)
    float rawCurrent;              // Roher ungefilterter Strom (A)
    float currentPower;            // Aktuelle Leistung (W)
    float currentOffset;           // Kalibrierungs-Offset für Nullpunkt
    
    // Energiezähler
    float consumedMAh;             // Verbrauchte Energie in mAh
    float consumedWh;              // Verbrauchte Energie in Wh
    unsigned long lastEnergyUpdate; // Letztes Update für Energieberechnung
    
    // Moving Average Filter (Spannung)
    static const uint8_t VOLTAGE_FILTER_SAMPLES = 10;
    float voltageBuffer[VOLTAGE_FILTER_SAMPLES];
    uint8_t voltageBufferIndex;
    bool voltageBufferFilled;
    
    // Moving Average Filter (Strom)
    static const uint8_t CURRENT_FILTER_SAMPLES = 20;
    float currentBuffer[CURRENT_FILTER_SAMPLES];
    uint8_t currentBufferIndex;
    bool currentBufferFilled;
    
    // Timing
    unsigned long lastUpdateTime;
    unsigned long lastWarningTime;
    unsigned long lastCurrentWarningTime;
    
    // Status-Flags
    bool warningActive;            // Spannungs-Warnung wurde ausgegeben
    bool criticalActive;           // Kritischer Zustand aktiv
    bool currentWarningActive;     // Strom-Warnung aktiv
    
    // Callbacks
    BatteryWarningCallback warningCallback;
    BatteryShutdownCallback shutdownCallback;
    CurrentWarningCallback currentWarningCallback;
    
    /**
     * Rohe ADC-Spannung auslesen
     * @return Spannung in Volt
     */
    float readRawVoltage();
    
    /**
     * Rohen ADC-Strom auslesen
     * @return Strom in Ampere
     */
    float readRawCurrent();
    
    /**
     * Spannung filtern (Moving Average)
     * @param newVoltage Neue Messung
     * @return Gefilterte Spannung
     */
    float filterVoltage(float newVoltage);
    
    /**
     * Strom filtern (Moving Average)
     * @param newCurrent Neue Messung
     * @return Gefilterter Strom
     */
    float filterCurrent(float newCurrent);
    
    /**
     * Spannung in Prozent umrechnen
     * @param voltage Spannung in Volt
     * @return Prozent (0-100)
     */
    uint8_t voltageToPercent(float voltage);
    
    /**
     * Energie-Verbrauch aktualisieren
     */
    void updateEnergyConsumption();
    
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