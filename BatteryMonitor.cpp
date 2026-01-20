/**
 * BatteryMonitor.cpp
 * 
 * Implementation des Batterie-Monitors mit ACS712-20A Stromsensor
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
    , currentCurrent(0.0f)
    , rawCurrent(0.0f)
    , currentPower(0.0f)
    , currentOffset(0.0f)
    , consumedMAh(0.0f)
    , consumedWh(0.0f)
    , lastEnergyUpdate(0)
    , voltageBufferIndex(0)
    , voltageBufferFilled(false)
    , currentBufferIndex(0)
    , currentBufferFilled(false)
    , lastUpdateTime(0)
    , lastWarningTime(0)
    , lastCurrentWarningTime(0)
    , warningActive(false)
    , criticalActive(false)
    , currentWarningActive(false)
    , warningCallback(nullptr)
    , shutdownCallback(nullptr)
    , currentWarningCallback(nullptr)
{
    // Buffer initialisieren
    for (uint8_t i = 0; i < VOLTAGE_FILTER_SAMPLES; i++) {
        voltageBuffer[i] = 0.0f;
    }
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; i++) {
        currentBuffer[i] = 0.0f;
    }
}

BatteryMonitor::~BatteryMonitor() {
}

bool BatteryMonitor::begin() {
    DEBUG_PRINTLN("BatteryMonitor: Initialisiere Spannungs- und Stromsensor...");
    
    setAutoShutdown(userConfig.getAutoShutdownEnabled());

    // ADC-Pins konfigurieren
    pinMode(VOLTAGE_SENSOR_PIN, INPUT);
    pinMode(CURRENT_SENSOR_PIN, INPUT);
    
    // ADC-Auflösung setzen (12-Bit)
    analogReadResolution(12);
    
    // Erste Spannungsmessung und Buffer füllen
    float initialVoltage = readRawVoltage();
    for (uint8_t i = 0; i < VOLTAGE_FILTER_SAMPLES; i++) {
        voltageBuffer[i] = initialVoltage;
    }
    voltageBufferFilled = true;
    
    currentVoltage = initialVoltage;
    rawVoltage = initialVoltage;
    currentPercent = voltageToPercent(initialVoltage);
    
    // Stromsensor kalibrieren (Nullpunkt)
    DEBUG_PRINTLN("BatteryMonitor: Kalibriere Stromsensor...");
    calibrateCurrent(100);
    
    // Erste Strommessung und Buffer füllen
    float initialCurrent = readRawCurrent();
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; i++) {
        currentBuffer[i] = initialCurrent;
    }
    currentBufferFilled = true;
    
    currentCurrent = initialCurrent;
    rawCurrent = initialCurrent;
    currentPower = currentVoltage * currentCurrent;
    
    lastEnergyUpdate = millis();
    initialized = true;
    
    DEBUG_PRINTLN("BatteryMonitor: ✅ Initialisiert");
    DEBUG_PRINTF("BatteryMonitor: Start-Spannung: %.2fV (%d%%)\n", currentVoltage, currentPercent);
    DEBUG_PRINTF("BatteryMonitor: Start-Strom: %.2fA (Offset: %.4fV)\n", currentCurrent, currentOffset);
    
    return true;
}

bool BatteryMonitor::update() {
    if (!initialized) return false;

    setAutoShutdown(userConfig.getAutoShutdownEnabled());

    // Nur alle VOLTAGE_CHECK_INTERVAL ms aktualisieren
    unsigned long now = millis();
    if (now - lastUpdateTime < VOLTAGE_CHECK_INTERVAL) {
        return false;
    }
    lastUpdateTime = now;
    
    // Spannungsmessung
    rawVoltage = readRawVoltage();
    currentVoltage = filterVoltage(rawVoltage);
    currentPercent = voltageToPercent(currentVoltage);
    
    // Strommessung
    rawCurrent = readRawCurrent();
    currentCurrent = filterCurrent(rawCurrent);
    
    // Leistung berechnen
    currentPower = currentVoltage * currentCurrent;
    
    // Energie-Verbrauch aktualisieren
    updateEnergyConsumption();
    
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

float BatteryMonitor::getCurrent() {
    return currentCurrent;
}

float BatteryMonitor::getRawCurrent() {
    return rawCurrent;
}

float BatteryMonitor::getPower() {
    return currentPower;
}

float BatteryMonitor::getConsumedMAh() {
    return consumedMAh;
}

float BatteryMonitor::getConsumedWh() {
    return consumedWh;
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

bool BatteryMonitor::isCurrentHigh() {
    return (currentCurrent >= CURRENT_WARNING);
}

void BatteryMonitor::calibrateCurrent(uint16_t samples) {
    DEBUG_PRINTF("BatteryMonitor: Kalibriere Stromsensor (%d Messungen)...\n", samples);
    
    float sum = 0.0f;
    
    for (uint16_t i = 0; i < samples; i++) {
        int adcValue = analogRead(CURRENT_SENSOR_PIN);
        float voltage = (adcValue / 4095.0f) * CURRENT_ADC_VREF;
        sum += voltage;
        delay(10);
    }
    
    currentOffset = sum / samples;
    
    DEBUG_PRINTF("BatteryMonitor: ✅ Nullpunkt: %.4fV\n", currentOffset);
}

void BatteryMonitor::resetEnergyCounters() {
    consumedMAh = 0.0f;
    consumedWh = 0.0f;
    lastEnergyUpdate = millis();
    DEBUG_PRINTLN("BatteryMonitor: Energiezähler zurückgesetzt");
}

void BatteryMonitor::setWarningCallback(BatteryWarningCallback callback) {
    warningCallback = callback;
}

void BatteryMonitor::setShutdownCallback(BatteryShutdownCallback callback) {
    shutdownCallback = callback;
}

void BatteryMonitor::setCurrentWarningCallback(CurrentWarningCallback callback) {
    currentWarningCallback = callback;
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
    
    delay(1000);
    
    // Deep Sleep ohne Wakeup = Permanentes Ausschalten
    esp_deep_sleep_start();
}

void BatteryMonitor::printInfo() {
    DEBUG_PRINTLN("\n╔════════════════════════════════════════╗");
    DEBUG_PRINTLN("║       BATTERY MONITOR INFO             ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════╝");
    DEBUG_PRINTF("Spannung:     %.2fV (raw: %.2fV)\n", currentVoltage, rawVoltage);
    DEBUG_PRINTF("Ladezustand:  %d%%\n", currentPercent);
    DEBUG_PRINTF("Strom:        %.2fA (raw: %.2fA)\n", currentCurrent, rawCurrent);
    DEBUG_PRINTF("Leistung:     %.2fW\n", currentPower);
    DEBUG_PRINTF("Verbraucht:   %.1fmAh / %.2fWh\n", consumedMAh, consumedWh);
    DEBUG_PRINTF("Status:       %s\n", 
        isCritical() ? "⚠️ KRITISCH" : 
        isLow() ? "⚡ LOW" : 
        isCurrentHigh() ? "⚡ HIGH CURRENT" :
        "✅ OK");
    DEBUG_PRINTF("Auto-Shutdown: %s\n", autoShutdownEnabled ? "aktiviert" : "deaktiviert");
    DEBUG_PRINTLN("────────────────────────────────────────");
    DEBUG_PRINTF("V-Min:        %.2fV (0%%)\n", VOLTAGE_BATTERY_MIN);
    DEBUG_PRINTF("V-Nominal:    %.2fV\n", VOLTAGE_BATTERY_NOM);
    DEBUG_PRINTF("V-Max:        %.2fV (100%%)\n", VOLTAGE_BATTERY_MAX);
    DEBUG_PRINTF("V-Warnung:    %.2fV\n", VOLTAGE_ALARM_LOW);
    DEBUG_PRINTF("V-Shutdown:   %.2fV\n", VOLTAGE_SHUTDOWN);
    DEBUG_PRINTF("I-Warnung:    %.1fA\n", CURRENT_WARNING);
    DEBUG_PRINTF("I-Max:        %.1fA\n", CURRENT_MAX);
    DEBUG_PRINTF("I-Offset:     %.4fV\n", currentOffset);
    DEBUG_PRINTLN("╚════════════════════════════════════════╝\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// PRIVATE METHODEN
// ═══════════════════════════════════════════════════════════════════════════

float BatteryMonitor::readRawVoltage() {
    int adcValue = analogRead(VOLTAGE_SENSOR_PIN);
    float voltage = (VOLTAGE_RANGE_MAX / 4095.0f) * float(adcValue);
    voltage *= VOLTAGE_CALIBRATION_FACTOR;
    return voltage;
}

float BatteryMonitor::readRawCurrent() {
    int adcValue = analogRead(CURRENT_SENSOR_PIN);
    
    // ADC zu Spannung (0-3.3V)
    float voltage = (adcValue / 4095.0f) * CURRENT_ADC_VREF;
    
    // Offset abziehen (Nullpunkt bei 1.65V für 3.3V-Versorgung)
    float deltaV = voltage - currentOffset;
    
    // In Ampere umrechnen (Sensitivität: ~66mV/A bei 3.3V)
    float current = deltaV / CURRENT_SENSITIVITY;
    
    // Negative Werte auf 0 setzen (wir messen nur Verbrauch, kein Laden)
    if (current < 0.0f) current = 0.0f;
    
    return current;
}

float BatteryMonitor::filterVoltage(float newVoltage) {
    voltageBuffer[voltageBufferIndex] = newVoltage;
    voltageBufferIndex = (voltageBufferIndex + 1) % VOLTAGE_FILTER_SAMPLES;
    
    float sum = 0.0f;
    for (uint8_t i = 0; i < VOLTAGE_FILTER_SAMPLES; i++) {
        sum += voltageBuffer[i];
    }
    
    return sum / VOLTAGE_FILTER_SAMPLES;
}

float BatteryMonitor::filterCurrent(float newCurrent) {
    currentBuffer[currentBufferIndex] = newCurrent;
    currentBufferIndex = (currentBufferIndex + 1) % CURRENT_FILTER_SAMPLES;
    
    float sum = 0.0f;
    for (uint8_t i = 0; i < CURRENT_FILTER_SAMPLES; i++) {
        sum += currentBuffer[i];
    }
    
    return sum / CURRENT_FILTER_SAMPLES;
}

uint8_t BatteryMonitor::voltageToPercent(float voltage) {
    voltage = constrain(voltage, VOLTAGE_BATTERY_MIN, VOLTAGE_BATTERY_MAX);
    
    float percent = ((voltage - VOLTAGE_BATTERY_MIN) / 
                     (VOLTAGE_BATTERY_MAX - VOLTAGE_BATTERY_MIN)) * 100.0f;
    
    return constrain((uint8_t)percent, 0, 100);
}

void BatteryMonitor::updateEnergyConsumption() {
    unsigned long now = millis();
    
    if (lastEnergyUpdate == 0) {
        lastEnergyUpdate = now;
        return;
    }
    
    // Zeit seit letztem Update in Stunden
    float deltaTimeHours = (now - lastEnergyUpdate) / 3600000.0f;
    
    // Energie = Strom × Zeit
    consumedMAh += currentCurrent * 1000.0f * deltaTimeHours;  // mAh
    consumedWh += currentPower * deltaTimeHours;                // Wh
    
    lastEnergyUpdate = now;
}

void BatteryMonitor::checkWarnings() {
    unsigned long now = millis();
    
    // Low-Battery Warnung (alle 10 Sekunden)
    if (isLow() && !warningActive) {
        if (now - lastWarningTime >= 10000) {
            DEBUG_PRINTLN("\n⚡ WARNUNG: Batteriespannung niedrig!");
            DEBUG_PRINTF("   Spannung: %.2fV (%d%%)\n", currentVoltage, currentPercent);
            
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
    
    // High-Current Warnung (alle 5 Sekunden)
    if (isCurrentHigh() && !currentWarningActive) {
        if (now - lastCurrentWarningTime >= 5000) {
            DEBUG_PRINTLN("\n⚡ WARNUNG: Hoher Stromverbrauch!");
            DEBUG_PRINTF("   Strom: %.2fA, Leistung: %.2fW\n", currentCurrent, currentPower);
            
            if (currentWarningCallback != nullptr) {
                currentWarningCallback(currentCurrent, currentPower);
            }
            
            lastCurrentWarningTime = now;
            currentWarningActive = true;
        }
    }
    
    // Strom-Warnung zurücksetzen
    if (!isCurrentHigh() && currentWarningActive) {
        currentWarningActive = false;
        DEBUG_PRINTLN("✅ Stromverbrauch wieder normal");
    }
}

void BatteryMonitor::checkShutdown() {
    if (isCritical() && !criticalActive) {
        criticalActive = true;
        DEBUG_PRINTLN("\n⚠️⚠️⚠️ KRITISCHE UNTERSPANNUNG! ⚠️⚠️⚠️");
        shutdown();
    }
}