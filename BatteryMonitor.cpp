/**
 * BatteryMonitor.cpp
 * 
 * Implementation des Batterie-Monitors
 */

#include "include/Globals.h"
#include "include/BatteryMonitor.h"
#include "include/UserConfig.h"

BatteryMonitor::BatteryMonitor()
    : initialized(false)
    , autoShutdownEnabled(true)
    , currentVoltage(0.0f)
    , rawVoltage(0.0f)
    , currentPercent(0)
    , bufferIndex(0)
    , bufferFilled(false)
    , lastUpdateTime(0)
    , lastWarningTime(0)
    , warningActive(false)
    , criticalActive(false)
    , warningCallback(nullptr)
    , shutdownCallback(nullptr)
{
    // Buffer initialisieren
    for (uint8_t i = 0; i < FILTER_SAMPLES; i++) {
        voltageBuffer[i] = 0.0f;
    }
}

BatteryMonitor::~BatteryMonitor() {
    // Cleanup falls nötig
}

bool BatteryMonitor::begin() {
    DEBUG_PRINTLN("BatteryMonitor: Initialisiere Spannungssensor...");
    
    setAutoShutdown(userConfig.getAutoShutdown());

    // ADC-Pin konfigurieren
    pinMode(VOLTAGE_SENSOR_PIN, INPUT);
    
    // ADC-Auflösung setzen (12-Bit)
    analogReadResolution(12);
    
    // Erste Messung durchführen und Buffer füllen
    float initialVoltage = readRawVoltage();
    for (uint8_t i = 0; i < FILTER_SAMPLES; i++) {
        voltageBuffer[i] = initialVoltage;
    }
    bufferFilled = true;
    
    currentVoltage = initialVoltage;
    rawVoltage = initialVoltage;
    currentPercent = voltageToPercent(initialVoltage);
    
    initialized = true;
    
    DEBUG_PRINTLN("BatteryMonitor: ✅ Initialisiert");
    DEBUG_PRINTF("BatteryMonitor: Start-Spannung: %.2fV (%d%%)\n", currentVoltage, currentPercent);
    
    return true;
}

bool BatteryMonitor::update() {
    if (!initialized) return false;

    setAutoShutdown(userConfig.getAutoShutdown());

    // Nur alle VOLTAGE_CHECK_INTERVAL ms aktualisieren
    unsigned long now = millis();
    if (now - lastUpdateTime < VOLTAGE_CHECK_INTERVAL) {
        return false;
    }
    lastUpdateTime = now;
    
    // Neue Messung
    rawVoltage = readRawVoltage();
    currentVoltage = filterVoltage(rawVoltage);
    currentPercent = voltageToPercent(currentVoltage);
    
    // Warnungen prüfen
    checkWarnings();
    
    // Shutdown prüfen (falls aktiviert)
    if (autoShutdownEnabled) {
        checkShutdown();
    }
    
    return true;
}

float BatteryMonitor::getVoltage() {
    return currentVoltage;
}

float BatteryMonitor::getRawVoltage() {
    return rawVoltage;
}

uint8_t BatteryMonitor::getPercent() {
    return currentPercent;
}

bool BatteryMonitor::isCritical() {
    return (currentVoltage <= VOLTAGE_SHUTDOWN);
}

bool BatteryMonitor::isLow() {
    return (currentVoltage <= VOLTAGE_ALARM_LOW);
}

void BatteryMonitor::setWarningCallback(BatteryWarningCallback callback) {
    warningCallback = callback;
}

void BatteryMonitor::setShutdownCallback(BatteryShutdownCallback callback) {
    shutdownCallback = callback;
}

void BatteryMonitor::setAutoShutdown(bool enabled) {
    autoShutdownEnabled = enabled;
}

void BatteryMonitor::shutdown() {
    DEBUG_PRINTLN("\n╔════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  ⚠️  BATTERY SHUTDOWN - UNTERSPANNUNG  ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════╝");
    DEBUG_PRINTF("Spannung: %.2fV (Limit: %.2fV)\n", currentVoltage, VOLTAGE_SHUTDOWN);
    DEBUG_PRINTLN("ESP32 fährt herunter...\n");
    
    // Shutdown-Callback aufrufen (falls gesetzt)
    if (shutdownCallback != nullptr) {
        shutdownCallback(currentVoltage);
    }
    
    delay(1000);  // Zeit für letzte Aktionen
    
    // Deep Sleep ohne Wakeup = Permanentes Ausschalten
    esp_deep_sleep_start();
}

void BatteryMonitor::printInfo() {
    DEBUG_PRINTLN("\n╔════════════════════════════════════════╗");
    DEBUG_PRINTLN("║         BATTERY MONITOR INFO           ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════╝");
    DEBUG_PRINTF("Spannung:     %.2fV (raw: %.2fV)\n", currentVoltage, rawVoltage);
    DEBUG_PRINTF("Ladezustand:  %d%%\n", currentPercent);
    DEBUG_PRINTF("Status:       %s\n", 
        isCritical() ? "⚠️ KRITISCH" : 
        isLow() ? "⚡ LOW" : 
        "✅ OK");
    DEBUG_PRINTF("Auto-Shutdown: %s\n", autoShutdownEnabled ? "aktiviert" : "deaktiviert");
    DEBUG_PRINTLN("────────────────────────────────────────");
    DEBUG_PRINTF("Min:          %.2fV (0%%)\n", VOLTAGE_BATTERY_MIN);
    DEBUG_PRINTF("Nominal:      %.2fV\n", VOLTAGE_BATTERY_NOM);
    DEBUG_PRINTF("Max:          %.2fV (100%%)\n", VOLTAGE_BATTERY_MAX);
    DEBUG_PRINTF("Warnung:      %.2fV\n", VOLTAGE_ALARM_LOW);
    DEBUG_PRINTF("Shutdown:     %.2fV\n", VOLTAGE_SHUTDOWN);
    DEBUG_PRINTLN("╚════════════════════════════════════════╝\n");
}


float BatteryMonitor::readRawVoltage() {
    // ADC auslesen (12-Bit: 0-4095)
    int adcValue = analogRead(VOLTAGE_SENSOR_PIN);

    // In Spannung umrechnen
    float voltage = (VOLTAGE_RANGE_MAX / 4095.0f) * float(adcValue);
    voltage *= VOLTAGE_CALIBRATION_FACTOR;

    return voltage;
}

float BatteryMonitor::filterVoltage(float newVoltage) {
    // Neuen Wert in Buffer schreiben
    voltageBuffer[bufferIndex] = newVoltage;
    bufferIndex = (bufferIndex + 1) % FILTER_SAMPLES;
    
    // Durchschnitt berechnen
    float sum = 0.0f;
    for (uint8_t i = 0; i < FILTER_SAMPLES; i++) {
        sum += voltageBuffer[i];
    }
    
    return sum / FILTER_SAMPLES;
}

uint8_t BatteryMonitor::voltageToPercent(float voltage) {
    // Auf gültigen Bereich begrenzen
    voltage = constrain(voltage, VOLTAGE_BATTERY_MIN, VOLTAGE_BATTERY_MAX);
    
    // Linear auf 0-100% mappen
    float percent = ((voltage - VOLTAGE_BATTERY_MIN) / 
                     (VOLTAGE_BATTERY_MAX - VOLTAGE_BATTERY_MIN)) * 100.0f;
    
    return constrain((uint8_t)percent, 0, 100);
}

void BatteryMonitor::checkWarnings() {
    unsigned long now = millis();
    
    // Low-Battery Warnung (nur alle 10 Sekunden)
    if (isLow() && !warningActive) {
        if (now - lastWarningTime >= 10000) {
            DEBUG_PRINTLN("\n⚡ WARNUNG: Batteriespannung niedrig!");
            DEBUG_PRINTF("   Spannung: %.2fV (%d%%)\n", currentVoltage, currentPercent);
            
            // Callback aufrufen (falls gesetzt)
            if (warningCallback != nullptr) {
                warningCallback(currentVoltage, currentPercent);
            }
            
            lastWarningTime = now;
            warningActive = true;
        }
    }
    
    // Warnung zurücksetzen wenn Spannung wieder OK
    if (!isLow() && warningActive) {
        warningActive = false;
        DEBUG_PRINTLN("✅ Batteriespannung wieder OK");
    }
}

void BatteryMonitor::checkShutdown() {
    // Kritische Unterspannung erreicht?
    if (isCritical() && !criticalActive) {
        criticalActive = true;
        
        // Sofortiger Shutdown!
        DEBUG_PRINTLN("\n⚠️⚠️⚠️ KRITISCHE UNTERSPANNUNG! ⚠️⚠️⚠️");
        shutdown();
    }
}