# Master-Firmware

## 1. Übersicht

Der Master ist die zentrale Brücke zwischen dem ESP-NOW-Funknetz der Smarthome-Geräte
und dem MQTT-Server. Er empfängt Sensordaten und Events von den Geräten per ESP-NOW,
übersetzt sie in MQTT-Nachrichten und leitet Kommandos vom Server an die Geräte weiter.

**Hardware:** ESP32-C3  
**Rolle:** ESP-NOW ↔ MQTT Bridge, Node-Registry, Provisioning-Server  
**Funk:** ESP-NOW (Kanal 6) + WiFi (2,4 GHz)

## 2. Architektur

```
┌──────────┐  ESP-NOW   ┌──────────┐   MQTT    ┌──────────┐
│ Geräte   │◄─────────►│  Master  │◄────────►│  Server  │
│ (Nodes)  │  STATE,    │(ESP32-C3)│  JSON    │(Node-RED)│
│          │  EVENT,ACK │          │          │          │
└──────────┘            └──────────┘          └──────────┘
```

Der Master besteht aus fünf logischen Schichten:

| Schicht | Funktion | Implementierung |
|---------|----------|----------------|
| ESP-NOW Empfang | Funkpakete entgegennehmen | `onEspNowReceive()` → `verarbeiteEspNowPaket()` |
| Zustands-Parser | Payload interpretieren | `parseNetErlState()`, `parseNetZrlState()`, `parseNetSenState()`, `parseBatSenState()` |
| Node-Registry | Geräte verwalten | `NodeRuntime`-Array (16 Slots), NVS-Persistenz |
| MQTT-Brücke | JSON publizieren/empfangen | `publishNodeState()`, `publishNodeMeta()`, `mqttCallback()` |
| Kommando-Dispatch | Befehle weiterleiten | `handleMqttGetState()`, `handleMqttSetRelay()`, `handleMqttCoverCommand()`, `handleMqttSetConfig()` |

## 3. ESP-NOW Empfang

### 3.1 Paket-Validierung
Jedes eingehende ESP-NOW-Paket durchläuft folgende Prüfungen:
1. **MAC-Prüfung**: Sender-MAC muss 6 Byte lang sein
2. **CRC-Prüfung**: 16-Bit CRC über den Payload (Startwert 0xFFFF)
3. **Längen-Prüfung**: Payload muss mindestens `sizeof(MsgHeader)` Bytes haben
4. **Magic-Byte**: Erstes Byte muss `0xAA` sein (Protokoll-Erkennung)

### 3.2 Nachrichten-Typen
Der Master verarbeitet folgende ESP-NOW-Nachrichten:

| Typ | Handler | Beschreibung |
|-----|---------|-------------|
| `SH_MSG_HELLO` | `verarbeiteHello()` | Gerät meldet sich an, sendet Metadaten |
| `SH_MSG_STATE` | `verarbeiteStateReport()` | Sensordaten und Relais-Zustände |
| `SH_MSG_EVENT` | `verarbeiteEvent()` | Ereignisse (Button, Bewegung, Fenster) |
| `SH_MSG_ACK` | `verarbeiteAck()` | Bestätigung auf Kommando |
| `SH_MSG_HEARTBEAT` | `verarbeiteHeartbeat()` | Lebenszeichen (Uptime, Batterie) |

## 4. Zustands-Parser (verarbeiteStateReport)

Die Funktion `verarbeiteStateReport()` empfängt einen STATE-Payload und dispatched
an einen von vier geräteklassen-spezifischen Parsern:

```
verarbeiteStateReport()
├── node_id extrahieren + Node suchen/registrieren
├── device_class prüfen (ggf. HELLO anfordern)
├── switch(device_class):
│   ├── SH_CLASS_NET_ERL  → parseNetErlState()    (7 Payload-Varianten)
│   ├── SH_CLASS_NET_ZRL  → parseNetZrlState()    (2 Payload-Varianten)
│   ├── SH_CLASS_NET_SEN  → parseNetSenState()    (4 Payload-Varianten)
│   └── SH_CLASS_BAT_SEN  → parseBatSenState()    (2 Payload-Varianten)
└── Node-State sanitieren + MQTT publizieren
```

### 4.1 parseNetErlState() – 7 Payload-Varianten

| Größe | Payload-Typ | Enthaltene Felder |
|-------|------------|-------------------|
| 20 Byte | StateReportPayload | relay_1 |
| 22 Byte | StateConfigReportPayload | relay_1, report_interval_s |
| 27 Byte | RelayComfortStateReportPayload | + temp, hum, lux, motion |
| 31 Byte | RelayComfortConfigStateReportPayload | + auto_flags, fault |
| 37 Byte | ExtendedRelayComfortStateReportPayload | + pressure, gas (BME680) |
| 41 Byte | ExtendedRelayComfortConfigStateReportPayload | + aqi, tvoc, eco2 (ENS160) |
| 45 Byte | ExtendedRelayComfortGasConfigStateReportPayload | voller Gas-State |

**Disambiguierung bei 41 Byte:** Die beiden 41-Byte-Varianten (Config vs. Gas)
werden über `SH_CAP_LED_RING` aus den HELLO-Metadaten unterschieden:
- LED_Ring gesetzt → Config-Layout (kein Gassensor)
- LED_Ring nicht gesetzt → Gas-Layout (BME680)

## 5. Node-Registry

### 5.1 Registrierung
Der Master verwaltet bis zu 16 Geräte (`MAX_DYNAMIC_NODES`). Jedes Gerät wird
über einen `NodeRuntime`-Struct repräsentiert:

```
Registrierungs-Ablauf:
1. Unbekanntes Gerät sendet STATE → provisorische Registrierung
2. Master fordert HELLO an (Metadaten)
3. HELLO trifft ein → vollständige Registrierung
4. ohne HELLO: provisorischer Eintrag verfällt nach 10 Minuten
```

### 5.2 Persistenz
Die Node-Registry wird im NVS-Flash gespeichert:
- **Namespace**: `master_reg`
- **Key**: `nodes_v1`
- **Magic**: `0x53485231` ("SHR1")
- **Struktur**: `PersistedRegistry` mit 16 `PersistedNodeSlot`-Einträgen
- **Speicherung**: Bei jeder neuen HELLO (nur wenn Änderungen vorliegen)
- **Laden**: Beim Boot aus NVS, validiert per Magic+Version

### 5.3 Node-Zustand (NodeRuntime)
```cpp
struct NodeRuntime {
    bool     belegt;              // Slot ist belegt
    bool     mac_bekannt;         // MAC-Adresse bekannt
    bool     meta_bekannt;        // HELLO-Metadaten empfangen
    bool     state_bekannt;       // STATE-Payload empfangen
    bool     online;              // Gerät ist online (< 75s ohne Kontakt)
    bool     master_mac_gueltig;  // MAC ist als Master gebunden
    uint8_t  device_class;        // SH_CLASS_NET_ERL/_ZRL/_SEN/_BAT_SEN
    uint16_t caps;                // Fähigkeiten-Bitmaske
    uint8_t  auto_flags;          // Auto-Light-Status (nur NET_ERL)
    // ... weitere Felder
};
```

## 6. MQTT-Brücke

### 6.1 MQTT-Befehle (Broker → Master)

Der Master abonniert `smarthome/device/+/command` und dispatched Kommandos
über folgende Handler:

| Handler | Kommando | Aktion |
|---------|----------|--------|
| `handleMqttGetState()` | `get_state` | STATE-Request per ESP-NOW an Gerät senden |
| `handleMqttSetRelay()` | `set_relay` | Relais-Befehl per ESP-NOW an Gerät senden |
| `handleMqttCoverCommand()` | `open`/`close`/`stop`/`set_position` | Cover-Befehl per ESP-NOW senden |
| `handleMqttSetConfig()` | `set_config` | Konfigurations-Befehl per ESP-NOW senden |

### 6.2 Pending-System
Jeder Befehl durchläuft das Pending-System:

```
1. ESP-NOW senden (mit ACK-Request-Flag)
2. Pending-Slot füllen (P pendingCmdRequest / PendingConfigRequest)
3. Warten auf ACK (COMMAND_ACK_TIMEOUT_MS = 800 ms)
4. Bei Timeout: Retry (max. 2 Wiederholungen)
5. Bei endgültigem Timeout: Fehler-ACK an Server
```

Der `PendingHeader`-Struct wird von CMD und CFG gemeinsam genutzt:
```cpp
struct PendingHeader {
    bool          aktiv;
    uint8_t       seq;               // Sequenznummer für ACK-Zuordnung
    uint8_t       retries;           // Anzahl Wiederholungen
    unsigned long letztes_senden_ms; // Zeitstempel letzter Versand
    char          request_id[48];    // MQTT-Request-ID
    char          command_channel[32]; // Kommando-Kanal
};
```

### 6.3 State-Publishing
Der Master publiziert pro Gerät:

| Topic | Retain | Inhalt |
|-------|:------:|--------|
| `smarthome/device/{id}/meta` | ✅ | Metadaten (Caps, Name, Version) |
| `smarthome/device/{id}/availability` | ✅ | Online-Status |
| `smarthome/device/{id}/state` | ✅ | Live-Zustand (Sensoren, Relais, Cover) |
| `smarthome/device/{id}/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/{id}/event` | ❌ | Ereignisse (via Event-Payload) |
| `smarthome/master/{id}/status` | ✅ | Master-Eigenstatus |
| `smarthome/master/{id}/event` | ❌ | Master-Ereignisse |

Bei MQTT-Reconnect werden alle bekannten Nodes erneut publiziert.

## 7. Kommando-Dispatch (mqttCallback)

Die `mqttCallback()`-Funktion wurde zur besseren Wartbarkeit in vier
geräteklassen-spezifische Handler extrahiert:

```
mqttCallback()
├── Payload parsen + Topic extrahieren
├── handleMqttGetState()        → STATE-Request senden
├── handleMqttSetRelay()        → Relais 1/2 schalten
├── handleMqttCoverCommand()    → Cover open/close/stop/set_position
└── handleMqttSetConfig()       → Konfiguration (master_mac, Intervalle)
```

## 8. Provisioning / Setup-Modus

Der Master kann in einen Setup-Modus versetzt werden (Taster 5s halten).
Im Setup-Modus:
- WiFi-AP wird gestartet (SSID: Geräte-ID)
- Webinterface zur Konfiguration (Master-MAC, MQTT-Broker)
- Normale Bridge-Funktion pausiert

Nach Verlassen des Setup-Modus wird die Konfiguration im NVS gespeichert
und die Bridge-Funktion mit neuer Konfiguration fortgesetzt.

## 9. Hilfsfunktionen

| Funktion | Zweck |
|----------|-------|
| `boolStr()` | `bool` → `"true"`/`"false"` (für JSON) |
| `macToText()` | `uint8_t[6]` → `"AA:BB:CC:DD:EE:FF"` |
| `fuellePendingHeader()` | Pending-Slot nach ESP-NOW-Send initialisieren |
| `inferDeviceClassFromState()` | Geräteklasse aus Payload-Größe und ID-Präfix ableiten |
| `sanitisiereNodeStateNachCapabilities()` | Nicht unterstützte Felder auf Sentinels zurücksetzen |

## 10. Konfiguration (AppConfig.h)

| Konstante | Wert | Bedeutung |
|-----------|------|-----------|
| `MASTER_WLAN_CHANNEL` | 6 | WiFi + ESP-NOW Kanal |
| `MASTER_COMMAND_ACK_TIMEOUT_MS` | 800 ms | Wartezeit auf ACK |
| `MASTER_COMMAND_MAX_RETRIES` | 2 | Max. Wiederholungen |
| `MASTER_NODE_OFFLINE_TIMEOUT_MS` | 75 s | Netzgerät gilt als offline |
| `MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS` | 600 s | Batteriegerät offline |
| `MASTER_MAX_DYNAMIC_NODES` | 16 | Max. registrierte Geräte |
| `MASTER_LOOP_INTERVAL_MS` | 10 ms | Hauptschleifen-Takt |

## 11. Logging-Konvention

| Level | Verwendung |
|-------|-----------|
| INFO | Normalbetrieb, Verbindungsaufbau, State-Wechsel |
| WARN | Transiente Fehler (Retry möglich), unerwartete Payloads |
| ERROR | Kritische Fehler (ESP-NOW/MQTT-Init fehlgeschlagen) |
