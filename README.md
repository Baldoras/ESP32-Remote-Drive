# ESP32-Remote-Drive

Ferngesteuerte Raupenfahrzeug-Plattform basierend auf ESP32-S3 mit dualer Projektarchitektur und ESP-NOW Wireless-Kommunikation.

## 🎯 Projekt-Übersicht

ESP32-Remote-Drive ist die **Fahrzeug-Seite** (Slave) eines Remote-Control-Systems:
- **ESP32-Remote-UI** (Master, separates Repo): Controller mit Display und Joystick
- **ESP32-Remote-Drive** (Slave, dieses Repo): Fahrzeugsteuerung mit Motoren und Telemetrie

> **Status**: Work in Progress - Aktive Entwicklung  
> **Hardware**: Getestet mit ESP32-S3-N16R8  
> **Partner-Projekt**: Benötigt ESP32-Remote-UI als Controller

### Kommunikation
- **Protokoll**: ESP-NOW mit TLV-Format (2.4 GHz, low-latency)
- **Reichweite**: ~100m (Freifeld)
- **Latenz**: <10ms typisch
- **Pairing**: Automatisch mit MAC-Validierung

## ⚡ Features

### Hardware-Unterstützung
- **Motoren**: L298N/DRV8833 H-Bridge mit Differential Steering
- **Batterie**: 4S2P 18650 Li-Ion (12.8V - 16.8V) mit BMS
- **Spannungssensor**: Voltage Sensor Module (max. 25V) mit Kalibrierung
- **Stromüberwachung**: ACS712-20A Current Sensor (3.3V)
- **Speicher**: SD-Karte für Logging und Konfiguration

### Software-Features
- ✅ Differential Steering für präzise Lenkung
- ✅ 200ms Joystick Timeout mit Auto-Stop (keine Bewegungsdaten)
- ✅ 2000ms ESP-NOW Connection Timeout (kompletter Verbindungsverlust)
- ✅ Battery Monitoring mit Auto-Shutdown bei 12.8V
- ✅ SD-Card Logging (Battery, Boot, Connection, Error, Motor)
- ✅ Serial Command Interface für Debugging
- ✅ JSON-basierte Konfiguration
- ✅ ESP-NOW TLV-Protokoll mit Builder-Pattern
- ✅ FreeRTOS Thread-Safe Operations

## 🔧 Hardware-Setup

### ESP32-Remote-Drive (Fahrzeug)

#### Pin-Belegung

**Motoren (L298N/DRV8833)**
```
Motor Links:  ENA=GPIO10, IN1=GPIO11, IN2=GPIO12
Motor Rechts: ENB=GPIO13, IN3=GPIO9,  IN4=GPIO16
PWM: 20 kHz, 8-Bit (0-255)
```

**SD-Karte (VSPI)**
```
CS=GPIO38, MOSI=GPIO40, MISO=GPIO41, SCK=GPIO39
SPI: 10 MHz
```

**Sensoren**
```
Spannungssensor: GPIO4 (ADC, 12-Bit)
  - Voltage Sensor Module (0-25V max)
  - Kalibrierungsfaktor: 0.7 (Hardware-abhängig)
  - Filtert mit Moving Average (10 Samples)
Current Sensor:  ACS712-20A @ 3.3V
```

#### Batterie-Spezifikationen
- **Typ**: 4S2P 18650 Li-Ion mit BMS
- **Spannung**: 12.8V - 16.8V (3.2V - 4.2V/Zelle)
- **Nominal**: 14.8V (3.7V/Zelle)
- **Warnung**: <13.2V (3.3V/Zelle)
- **Shutdown**: 12.8V (3.2V/Zelle) - konservativ für Lebensdauer!
- **Kapazität**: 2x parallele Strings für höhere Laufzeit
- **Sensor Range**: 0-25V (Voltage Sensor Module)
- **Kalibrierung**: Faktor 0.7 in `setupConf.h` (Hardware-abhängig!)

> **Hinweis**: Software-Shutdown bei 3.2V/Zelle maximiert Akku-Lebensdauer (2000+ Zyklen). BMS bietet zusätzlichen Schutz bei ~2.5V/Zelle. **Kalibrierungsfaktor muss ggf. angepasst werden!**

## 📦 Software-Architektur

### Module

```
ESP32-Remote-Drive/
├── ESP32-Remote-Drive.ino           # Hauptprogramm
├── Globals.cpp/h                    # Globale Instanzen
├── MotorController.cpp/h            # Differential Steering
├── BatteryMonitor.cpp/h             # Spannungs-/Stromüberwachung
├── PowerManager.cpp/h               # Sleep & Shutdown
├── ESPNowManager.cpp/h              # Basis ESP-NOW Kommunikation
├── ESPNowPacket.cpp/h               # TLV-Protokoll Paket-Klasse
├── ESPNowRemoteController.cpp/h     # Drive-spezifische ESP-NOW Logik mit Pairing
├── LogHandler.cpp/h                 # SD-Card Logging
├── SDCardHandler.cpp/h              # SD-Card I/O
├── SerialCommandHandler.cpp/h       # Debug-Interface
├── UserConfig.cpp/h                 # JSON-Config
└── ConfigManager.cpp/h              # Config-Framework
```

**Wichtig**: 
- `ESPNowPacket` ist eine **separate Klasse** für TLV-Protokoll
- `ESPNowRemoteController` erweitert `ESPNowManager` um Pairing und MAC-Validierung

### Differential Steering

Der MotorController implementiert echtes Differential Steering:

```cpp
// Joystick Input: X=[-100,+100], Y=[-100,+100]
leftSpeed  = Y + X  // Vorwärts + Rechtsdrehung erhöht links
rightSpeed = Y - X  // Vorwärts + Rechtsdrehung verringert rechts
```

**Features**:
- Kreisradius-Begrenzung (max. 100% Auslenkung)
- PWM-Mapping auf 127-255 für schwache Motoren
- Telemetrie: Speed (-100 bis +100) & PWM (0-255)

### ESP-NOW Protokoll

**TLV-Format** (Type-Length-Value):
```
[MAIN_CMD 1B][TOTAL_LEN 1B][SUB_CMD 1B][LEN 1B][DATA...][SUB_CMD][LEN][DATA]...
```

**Architektur**:
- `ESPNowPacket`: Standalone TLV-Paket-Klasse mit Builder & Parser
- `ESPNowManager`: Basis-Kommunikation (WiFi, Queues, Callbacks)
- `ESPNowRemoteController`: Drive-spezifisch mit Pairing & MAC-Validierung

**Builder-Pattern Beispiel** (`ESPNowPacket`):
```cpp
ESPNowPacket packet;
packet.begin(MainCmd::DATA_REQUEST)
      .addInt16(DataCmd::JOYSTICK_X, joyX)
      .addInt16(DataCmd::JOYSTICK_Y, joyY);
      
// Über ESPNowRemoteController senden
espNowCtrl.send(peerMac, packet);
```

**Parser Beispiel**:
```cpp
void onESPNowDataReceived(const uint8_t* mac, MainCmd cmd, ESPNowPacket* packet) {
    int16_t joyX, joyY;
    if (packet->getInt16(DataCmd::JOYSTICK_X, joyX) && 
        packet->getInt16(DataCmd::JOYSTICK_Y, joyY)) {
        motorCtrl.processMovementInput(joyX, joyY);
    }
}
```

**Pairing-Prozess**:
1. Remote-UI sendet `PAIR_REQUEST` mit MAC
2. `ESPNowRemoteController` validiert MAC
3. Bei Erfolg: Auto-Add als Peer + `PAIR_RESPONSE`
4. Heartbeat-basierte Verbindungsüberwachung startet

## 🚀 Installation

### Voraussetzungen
- **Arduino IDE** 1.8.19+ (PlatformIO nicht kompatibel mit ESP32 Core 3.3.0+)
- **ESP32 Core** 3.3.0+ (kritisch für neue PWM API!)
- **Bibliotheken**:
  - ArduinoJson 6+
  - ESP32 Core (WiFi, ESP-NOW)

### ESP32 Core Installation

Arduino IDE → Boards Manager → ESP32 by Espressif Systems → Version 3.3.0+

> **Wichtig**: ESP32 Core 3.3.0+ verwendet neue PWM-API (`ledcAttach()` statt `ledcSetup()`). Code ist **nicht** mit älteren Versionen kompatibel!

### Projekt klonen

```bash
git clone https://github.com/Baldoras/ESP32-Remote-Drive.git
cd ESP32-Remote-Drive
```

> **Hinweis**: Das Gegenstück **ESP32-Remote-UI** (Master/Controller) ist ein separates Repository.

### Konfiguration

Alle Hardware-Pins sind in `include/setupConf.h` definiert. Benutzerspezifische Einstellungen in `include/userConf.h`:

**Wichtige Parameter**:
```cpp
// ESP-NOW
#define ESPNOW_CHANNEL            0      // WiFi-Kanal (0=auto)
#define ESPNOW_HEARTBEAT_INTERVAL 500    // ms
#define ESPNOW_TIMEOUT            2000   // ms

// Battery
#define VOLTAGE_BATTERY_MIN       12.8   // Minimum (3.2V/Zelle)
#define VOLTAGE_SHUTDOWN          12.8   // Auto-Shutdown
#define VOLTAGE_ALARM_LOW         13.2   // Warnung

// Joystick (für UI-Projekt)
#define JOY_DEADZONE_PERCENT      5      // Deadzone %
#define JOY_INVERT_X              true   // X invertieren
#define JOY_INVERT_Y              true   // Y invertieren
```

### Flashen

1. Board auswählen: **ESP32-S3 Dev Module**
2. Port auswählen (z.B. COM3 / /dev/ttyUSB0)
3. Upload-Speed: **921600**
4. Flashen

## 📝 Verwendung

### Erster Start

1. SD-Karte einlegen (FAT32 formatiert)
2. ESP32 mit Strom versorgen
3. System initialisiert und wartet auf Remote-UI
4. Logs werden auf SD-Karte geschrieben

**Verbindungsstatus** über Serial Monitor (115200 Baud) prüfen.

### Serial Command Interface

Über Serial Monitor (115200 Baud) verfügbar:

```
help                    # Alle Befehle anzeigen
logs                    # Log-Dateien auflisten
read battery.log        # Log-Datei lesen
tail error.log 20       # Letzte 20 Zeilen
config                  # Konfiguration anzeigen
config set espnowChannel 6  # Parameter ändern
config save             # Speichern
battery                 # Batterie-Status
espnow                  # ESP-NOW Status
sysinfo                 # System-Info
```

### Pairing mit Remote-UI

Das Drive-System verwendet `ESPNowRemoteController` für automatisches Pairing:

1. Drive wartet auf `PAIR_REQUEST` vom Remote-UI
2. MAC-Adresse wird validiert (konfigurierbar in `userConf.h`)
3. Bei erfolgreicher Validierung: Peer automatisch hinzugefügt
4. `PAIR_RESPONSE` wird gesendet
5. Verbindung hergestellt (prüfe Serial Monitor)

**Debug-Logging**:
```
[ESP-NOW] Peer XX:XX:XX:XX:XX:XX connected
[CONNECTION] Remote connected
```

**MAC-Konfiguration** in `include/userConf.h`:
```cpp
#define ESPNOW_PEER_MAC "11:22:33:44:55:66"  // Master MAC
```

### Motor-Kalibrierung

Falls Motoren schwach sind, PWM-Mapping anpassen in `MotorController.cpp`:

```cpp
// Standard: 0-255
int leftPWM = (int)(abs(leftSpeed) * 255.0 / 200.0);

// Für schwache Motoren: 127-255
int leftPWM = map(abs(leftSpeed), 0, 200, 127, 255);
```

## 🔍 Debugging

### Log-Dateien (SD-Karte)

```
/logs/
├── battery.log      # Spannungs-/Stromverlauf
├── boot.log         # System-Start & Initialisierung
├── connection.log   # ESP-NOW Events
├── error.log        # Fehler & Warnungen
└── motor.log        # Motor-Telemetrie
```

**Log-Format**: `[HH:MM:SS.mmm] [LEVEL] [TAG] Message`

### Häufige Probleme

**Motoren reagieren nicht**
- PWM-Pins korrekt? → `setupConf.h` prüfen
- Motor-Enable aktiviert? → `motorCtrl.enable()`
- Joystick-Deadzone zu groß? → `userConf.h`

**Kommunikation bricht ab**
- Timeout zu kurz? → `ESPNOW_TIMEOUT` erhöhen
- Heartbeat aktiviert? → `espNow.setHeartbeat(true, 500)`
- MAC-Adresse korrekt? → `espnow` Command

**Batterie-Shutdown zu früh**
- Spannungsgrenze anpassen: `VOLTAGE_SHUTDOWN` in `setupConf.h`
- **Kalibrierung wichtig**: `VOLTAGE_CALIBRATION_FACTOR` in `setupConf.h`
  - Tatsächliche Spannung mit Multimeter messen
  - Faktor berechnen: `Faktor = Multimeter_Wert / ADC_Wert`
  - Standard: 0.7 (Hardware-abhängig!)
- Auto-Shutdown deaktivieren: `battery.setAutoShutdown(false)`

**Spannungswerte stimmen nicht**
- Kalibrierungsfaktor prüfen und anpassen
- ADC-Werte über Serial Monitor beobachten
- Voltage Sensor Module korrekt angeschlossen?
- Spannungsteiler-Verhältnis des Moduls beachten

**SD-Karte nicht erkannt**
- FAT32 formatiert?
- Pins korrekt? → `setupConf.h`
- SPI-Frequenz zu hoch? → `SD_SPI_FREQUENCY` reduzieren

## 📊 Telemetrie

**DataCmd Definitionen** sind in `include/ESPNowPacket.h` definiert:
- `MainCmd`: Haupt-Befehle (HEARTBEAT, DATA_REQUEST, PAIR_REQUEST, etc.)
- `DataCmd`: Sub-Commands für Daten-Identifier (JOYSTICK_X, BATTERY_VOLTAGE, etc.)

### Datenfluss (alle 500ms)

Remote-UI → Drive:
- Joystick X/Y: `int16_t` via `DataCmd::JOYSTICK_X/Y`
- Button States: `uint8_t` via `DataCmd::BUTTON_STATE`

Drive → Remote-UI:
- Battery Voltage: `uint16_t` (mV) via `DataCmd::BATTERY_VOLTAGE`
- Battery Percent: `uint8_t` (%) via `DataCmd::BATTERY_PERCENT`
- Motor Speeds L/R: `int8_t` via `DataCmd::MOTOR_LEFT/RIGHT`
- RSSI: `int8_t` (dBm) via `DataCmd::RSSI`
- Connection Status: `uint8_t` via `DataCmd::CONNECTION`

### Performance

- **Latenz**: <10ms (ESP-NOW)
- **Update-Rate**: 20ms (50 Hz) Joystick
- **Telemetrie**: 500ms (2 Hz)
- **Heartbeat**: 500ms
- **Timeouts**:
  - **Joystick**: 200ms → Motor-Stop bei fehlenden Bewegungsdaten
  - **Connection**: 2000ms → Verbindungsverlust, kompletter Shutdown

## 🔒 Sicherheit

### Critical Features

1. **Joystick Timeout**: 200ms ohne Bewegungsdaten → Motor-Stop
2. **Connection Timeout**: 2000ms ohne ESP-NOW Pakete → Verbindungsverlust
3. **Battery Protection**: Auto-Shutdown bei 12.8V
4. **BMS Backup**: Hardware-Schutz bei ~10V (2.5V/Zelle)
5. **Heartbeat Monitoring**: 500ms Intervall für Verbindungsüberwachung

### Best Practices

- Regelmäßig Batterie-Logs prüfen
- Niemals unter 12.8V entladen
- Bei schwachem Signal sofort stoppen (Connection Timeout beachten)
- Serial Monitor für Fehlermeldungen überwachen

## 🛠️ Entwicklung

### Erweiterungen

**Neue Sensoren hinzufügen**:
1. DataCmd in `include/ESPNowPacket.h` definieren:
```cpp
enum class DataCmd : uint8_t {
    // ...
    CUSTOM_SENSOR = 0xA0
};
```
2. Sensor-Modul erstellen (`.cpp/.h`)
3. Telemetrie in `sendTelemetry()` ergänzen:
```cpp
void sendTelemetry() {
    ESPNowPacket packet;
    packet.begin(MainCmd::DATA_RESPONSE);
    packet.addUInt16(DataCmd::CUSTOM_SENSOR, sensorValue);
    espNowCtrl.broadcast(packet);
}
```

**Custom Pairing-Logik**:
Erweitere `ESPNowRemoteController::handlePairRequest()` in `ESPNowRemoteController.cpp`

**Neue Main-Commands**:
Definiere in `ESPNowPacket.h` und handle in `onESPNowDataReceived()`

## 📄 Lizenz

MIT License - siehe [LICENSE](LICENSE)

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/Baldoras/ESP32-Remote-Drive/issues)
- **Diskussionen**: [GitHub Discussions](https://github.com/Baldoras/ESP32-Remote-Drive/discussions)
- **Remote-UI Repo**: Separates Repository für Controller-Seite

## 🙏 Danksagungen

- ESP32 Community
- Arduino Framework
- FreeRTOS Team

---

**Version**: 1.0.0  
**ESP32 Core**: 3.3.0  
**Letztes Update**: Januar 2026