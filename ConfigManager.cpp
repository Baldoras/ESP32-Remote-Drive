/**
 * ConfigManager.cpp
 * 
 * Implementation der abstrakten Basis-Klasse
 */

#include "include/ConfigManager.h"
#include "include/setupConf.h"

ConfigManager::ConfigManager()
    : dirty(false)
{
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::load() {
    DEBUG_PRINTLN("ConfigManager: Lade Config...");
    
    // 1. Aus Storage laden (implementiert von Subklasse)
    String content;
    if (!loadFromStorage(content)) {
        DEBUG_PRINTLN("ConfigManager: ❌ loadFromStorage() fehlgeschlagen");
        return false;
    }
    
    // 2. JSON deserialisieren (implementiert von Subklasse)
    if (!deserializeFromJson(content)) {
        DEBUG_PRINTLN("ConfigManager: ❌ deserializeFromJson() fehlgeschlagen");
        return false;
    }
    
    // 3. Validieren (implementiert von Subklasse)
    if (!validate()) {
        DEBUG_PRINTLN("ConfigManager: ⚠️ Validierung korrigierte Werte");
    }
    
    dirty = false;
    
    DEBUG_PRINTLN("ConfigManager: ✅ Config geladen");
    return true;
}

bool ConfigManager::save() {
    DEBUG_PRINTLN("ConfigManager: Speichere Config...");
    
    // 1. Validieren vor dem Speichern (implementiert von Subklasse)
    validate();
    
    // 2. Zu JSON serialisieren (implementiert von Subklasse)
    String content;
    if (!serializeToJson(content)) {
        DEBUG_PRINTLN("ConfigManager: ❌ serializeToJson() fehlgeschlagen");
        return false;
    }
    
    // 3. In Storage speichern (implementiert von Subklasse)
    if (!saveToStorage(content)) {
        DEBUG_PRINTLN("ConfigManager: ❌ saveToStorage() fehlgeschlagen");
        return false;
    }
    
    dirty = false;
    
    DEBUG_PRINTLN("ConfigManager: ✅ Config gespeichert");
    return true;
}