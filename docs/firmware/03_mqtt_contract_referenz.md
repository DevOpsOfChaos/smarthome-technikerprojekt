# MQTT-Contract-Referenz

> **Hinweis:** Dieses Dokument beschreibt den MQTT-Vertrag der **Firmware-Linie** (ESP-NOW → Master → MQTT).
> Die hier verwendeten Geräte-IDs (z.B. `NET-ERL-001`, `NET-ZRL-001`) sind Firmware-IDs.
> Für die ESPHome-Linie (MQTT direkt, IDs wie `NET-ERL-010`, `NET-ERL-020`) siehe [ESPHome-Dokumentation](../esphome/README.md).

**Stand:** Firmware v1 · **Basis:** ESPHome Packages (`smarthome_contract_base.yaml`, `smarthome_command_ack.yaml`, `smarthome_cover_contract.yaml`)

---

## Inhaltsverzeichnis

1. [Überblick](#1-überblick)
2. [Topic-Struktur](#2-topic-struktur)
3. [Base-Contract – Meta & Verfügbarkeit](#3-base-contract--meta--verfügbarkeit)
4. [ACK-Contract – Befehlsbestätigung](#4-ack-contract--befehlsbestätigung)
5. [Cover-Contract – Zustand & Steuerung](#5-cover-contract--zustand--steuerung)
6. [Globale ESPHome-Internals](#6-globale-esphome-internals)
7. [Nachrichtenfluss-Diagramme](#7-nachrichtenfluss-diagramme)
8. [Events & Trigger](#8-events--trigger)

---

## 1. Überblick

Zwei alternative Implementierungen: **ESPHome** (MQTT direkt) und **Firmware** (ESP-NOW → Master → MQTT). Die MQTT-Contracts sind ESPHome-Package-Vorlagen (`.yaml`), die auf dem **Master** (ESP32 Gateway) laufen und folgende Funktionen bereitstellen:

| Contract | Datei | Zweck |
|---|---|---|
| **Base** | `smarthome_contract_base.yaml` | Meta-Info, Verfügbarkeit, Setup-Portal, OTA |
| **ACK** | `smarthome_command_ack.yaml` | Bestätigung von ESP-NOW-Befehlen als MQTT-JSON |
| **Cover** | `smarthome_cover_contract.yaml` | Cover-Zustand (STATE) und Steuerbefehle (CMD) |

### 1.1 Datenfluss

```
┌─────────┐  ESP-NOW   ┌──────────┐  MQTT        ┌─────────────┐
│ Knoten  │◄──────────►│  Master  │◄────────────►│  Server     │
│ (Node)  │  binär     │ (ESP32)  │  JSON         │ (HomeAssist)│
└─────────┘            └──────────┘              └─────────────┘
```

---

## 2. Topic-Struktur

Alle Topics folgen dem Schema:

```
smarthome/device/{device_id}/{topic_suffix}
```

| Platzhalter | Beschreibung |
|---|---|
| `{device_id}` | Geräte-ID (z.B. `NET-ZRL-001`) – wird durch ESPHome-Template ersetzt |

### 2.1 Topic-Übersicht

| Suffix | QoS | Retain | Richtung | Contract |
|---|---|---|---|---|
| `meta` | 0 | ✅ Ja | Knoten → Server | Base |
| `availability` | 0 | ✅ Ja | Knoten → Server | Base |
| `ack` | 0 | ❌ Nein | Knoten → Server | ACK |
| `state` | 0 | ✅ Ja | Knoten → Server | Cover |
| `command` | 0 | ❌ Nein | Server → Knoten | Cover |
| `hello` | 0 | ❌ Nein | Knoten → Server | Base |
| `heartbeat` | 0 | ❌ Nein | Knoten → Server | Base |
| `event` | 0 | ❌ Nein | Knoten → Server | Cover |

---

## 3. Base-Contract – Meta & Verfügbarkeit

### 3.1 `smarthome/device/{device_id}/meta` (Retain)

Wird beim Booten und bei jeder HELLO-Nachricht aktualisiert. Enthält die vollständige Gerätebeschreibung.

```json
{
  "device_id": "NET-ZRL-001",
  "device_name": "Rollladen Wohnzimmer",
  "device_class": "net_erl",
  "power_type": 0,
  "fw_version": 1.0,
  "caps": 10243,
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "meta_schema_version": 1,
  "control_mode": 5,
  "config_profile": 3,
  "reporting_mode": 1,
  "sensor_mask": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "input_mask": [0, 0, 0, 0, 0, 0]
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Eindeutige Geräte-ID (z.B. `NET-ZRL-001`) |
| `device_name` | string (32) | Anzeigename aus der Gerätekonfiguration |
| `device_class` | string | Device-Class (§1 DeviceTypes). Werte: `"net_erl"`, `"net_zrl"`, `"net_sen"`, `"bat_sen"`, `"master"` |
| `power_type` | int | `0` = MAINS, `1` = BATTERY |
| `fw_version` | float | Firmware-Version (major.minor) |
| `caps` | int | 16-Bit-Capability-Bitmask (§6 DeviceTypes) |
| `mac_address` | string | MAC-Adresse als `XX:XX:XX:XX:XX:XX` |
| `meta_schema_version` | int | Meta-Schema-Version (aktuell 1) |
| `control_mode` | int | Control-Mode (§3 DeviceTypes) |
| `config_profile` | int | Config-Profil (§4 DeviceTypes) |
| `reporting_mode` | int | Reporting-Mode (§5 DeviceTypes) |
| `sensor_mask` | array[11] | 11-Byte-Sensor-Bitmaske als Integer-Array |
| `input_mask` | array[6] | 6-Byte-Input-Bitmaske als Integer-Array |

### 3.2 `smarthome/device/{device_id}/availability` (Retain)

Wird bei jeder HELLO/HEARTBEAT-Nachricht aktualisiert. Zeigt an, ob das Gerät erreichbar ist.

```json
{
  "device_id": "NET-ZRL-001",
  "availability": "online",
  "online": true,
  "power_type": 0
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `availability` | string | `"online"` oder `"offline"` |
| `online` | bool | `true` = online, `false` = offline |
| `power_type` | int | `0` = MAINS, `1` = BATTERY |

**Offline-Erkennung:**
- **MAINS-Geräte:** Wenn kein STATE/HELLO innerhalb des `report_interval_s * 3` eingeht → `"offline"`
- **BATTERY-Geräte:** Wenn kein HEARTBEAT innerhalb des `wake_interval_s * 4` eingeht → `"offline"`

---

## 4. ACK-Contract – Befehlsbestätigung

### 4.1 `smarthome/device/{device_id}/ack` (nicht Retain)

Wird nach jedem ESP-NOW-ACK (oder ACK-Timeout) gesendet. Erlaubt dem Server die Nachverfolgung von Befehlen.

```json
{
  "device_id": "NET-ZRL-001",
  "request_id": 42,
  "channel": "command",
  "status": "OK",
  "status_code": 0,
  "ack_msg_type": 5,
  "ack_seq": 128,
  "source": "espnow"
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `request_id` | int | Vom Server mitgesendete Request-ID (für Zuordnung) |
| `channel` | string | Kanal (immer `"command"`) |
| `status` | string | Textuelle Status-Beschreibung (`"OK"`, `"ERROR"`, `"TIMEOUT"`) |
| `status_code` | int | Numerischer Status-Code (0 = OK, siehe Fehlercodes unten) |
| `ack_msg_type` | int | Message-Typ des quittierten Pakets (`5` = CMD) |
| `ack_seq` | int | Sequenznummer (0–255, zyklisch) |
| `source` | string | Quelle (`"espnow"`, `"timeout"`, `"internal"`) |

### 4.2 Status-Codes

| Code | Bedeutung | Beschreibung |
|---|---|---|
| `0` | OK | Befehl erfolgreich verarbeitet |
| `1` | ERROR | Allgemeiner Fehler |
| `2` | REJECTED | Befehl abgelehnt (z.B. unbekannte Device-ID) |
| `3` | REJECTED_FULL | Befehl abgelehnt – ESP-NOW-Warteschlange voll |
| `-1` | TIMEOUT | Kein ACK innerhalb des Timeouts empfangen |
| `-2` | UNSUPPORTED | Befehl wird von diesem Gerät nicht unterstützt |

### 4.3 ack_seq Mechanik

- `ack_seq` wird pro Gerät hochgezählt (0 → 1 → … → 255 → 0)
- Erlaubt Duplikatserkennung auf Empfängerseite
- Wird im ESP-NOW-AckPayload übertragen und hier gespiegelt

---

## 5. Cover-Contract – Zustand & Steuerung

### 5.1 `smarthome/device/{device_id}/state` (Retain)

Wird bei jeder STATE-NAChricht eines NET-ZRL im COVER-Mode gesendet.

```json
{
  "device_id": "NET-ZRL-001",
  "relay_1": false,
  "relay_2": false,
  "cover_mode": true,
  "cover_state": "stopped",
  "cover_position": 75,
  "cover_calibrated": true,
  "fault": 0
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `relay_1` | bool | Relais 1 Zustand (true = an) |
| `relay_2` | bool | Relais 2 Zustand (true = an) |
| `cover_mode` | bool | Cover aktiv (true = Cover-Modus) |
| `cover_state` | string | `"opening"`, `"closing"`, `"stopped"` |
| `cover_position` | int \| null | Position 0–100, `null` wenn nicht kalibriert |
| `cover_calibrated` | bool | Kalibrierungsstatus |
| `fault` | int | Fehlercode (0 = kein Fehler) |

**cover_position Wertebereich:**

| Wert | Bedeutung |
|---|---|
| `0` | Vollständig geschlossen / unten |
| `100` | Vollständig geöffnet / oben |
| `null` | Nicht kalibriert (Position unbekannt) |
| `1–99` | Zwischenposition |

### 5.2 `smarthome/device/{device_id}/command` (nicht Retain)

Eingehende Befehle vom Server zur Steuerung des Covers.

#### Allgemeines Format

```json
{
  "request_id": 42,
  "device_id": "NET-ZRL-001",
  "command": "set_position",
  "position": 50
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `request_id` | int | Optionale ID für ACK-Zuordnung |
| `device_id` | string | Ziel-Geräte-ID |
| `command` | string | Befehl (siehe Tabelle) |
| `*` | * | Befehls-spezifische Parameter |

#### Unterstützte Befehle

| Befehl | Parameter | Beschreibung |
|---|---|---|
| `get_state` | — | Sofortigen STATE anfordern |
| `set_config` | `{param_id, value}` | Konfigurationsparameter setzen |
| `calibrate` | — | Cover-Kalibrierung starten |
| `clear_calibration` | — | Kalibrierung zurücksetzen |
| `open` | — | Cover vollständig öffnen (Position 100) |
| `close` | — | Cover vollständig schließen (Position 0) |
| `stop` | — | Bewegung sofort stoppen |
| `set_position` | `position: 0–100` oder `value: 0–100` | Auf bestimmte Position fahren |

#### Beispiele

```json
// Cover öffnen
{ "request_id": 1, "device_id": "NET-ZRL-001", "command": "open" }

// Auf Position 50 fahren
{ "request_id": 2, "device_id": "NET-ZRL-001", "command": "set_position", "position": 50 }
{ "request_id": 2, "device_id": "NET-ZRL-001", "command": "set_position", "value": 50 }

// Kalibrierung starten
{ "request_id": 3, "device_id": "NET-ZRL-001", "command": "calibrate" }
```

### 5.3 Fehlercodes (Cover-Contract)

| Code | Bedeutung | Beschreibung |
|---|---|---|
| `-20` | `missing_request_id` | Request-ID fehlt im Befehl |
| `-21` | `invalid_payload` | Payload ungültig oder fehlerhaft |
| `-6` | `calib_in_progress` | Kalibrierung läuft bereits / Gerät bereits kalibriert |
| `-6` | `already_calibrated` | (gleicher Code wie calib_in_progress) |
| `-5` | `not_calibrated` | Cover nicht kalibriert – Position kann nicht angefahren werden |
| `-2` | `unsupported` | Befehl von diesem Gerät nicht unterstützt |

### 5.4 Kalibrierungs-Zustandsmaschine

```
Phase 0: Idle        ──calibrate──►  Phase 1: MovingUp
   ▲                                      │
   │                                      │ (oben angekommen)
   │                                      ▼
   │                              Phase 2: WaitForDown
   │                                      │
   │                                      │ (automatisch)
   │                                      ▼
   │                              Phase 3: MeasuringDown
   │                                      │
   │                                      │ (unten angekommen)
   │                                      ▼
   │                              Phase 4: WaitForSave
   │                                      │
   │                                      │ (save)
   │                                      ▼
   └─── save complete ────►       Phase 5: Complete
```

| Phase | Wert | Beschreibung |
|---|---|---|
| `Idle` | 0 | Ausgangszustand, keine Kalibrierung aktiv |
| `MovingUp` | 1 | Rollladen fährt hoch (oberen Endpunkt suchen) |
| `WaitForDown` | 2 | Oben angekommen, warte auf automatischen Start der Abfahrt |
| `MeasuringDown` | 3 | Rollladen fährt runter (Zeitmessung, unteren Endpunkt suchen) |
| `WaitForSave` | 4 | Unten angekommen, warte auf Speicherung |
| `Complete` | 5 | Kalibrierung abgeschlossen, `cover_calibrated = true` |

**Wichtig:** Zwischenpositionen (`set_position` mit 1–99) sind nur nach erfolgreicher Kalibrierung möglich. Vor der Kalibrierung sind nur `open` (100) und `close` (0) erlaubt.

### 5.5 `smarthome/device/{device_id}/state` – NET-ERL State Payload

Wird bei jeder STATE-Nachricht eines NET-ERL gesendet. Der Payload enthält den vollen Umfang des `ExtendedRelayComfortGasConfigStateReportPayload` (45 Bytes).

```json
{
  "device_id": "NET-ERL-001",
  "relay_1": false,
  "motion": false,
  "temperature": 21.5,
  "humidity": 48.2,
  "lux": 250,
  "auto_flags": 0,
  "fault": 0,
  "report_interval_s": 10,
  "auto_on_lux_threshold": 250
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `relay_1` | bool | Relais-Zustand (true = ein) |
| `motion` | bool | Präsenz erkannt (true = ja) |
| `temperature` | float | Temperatur in °C (`temp_01c / 10`) |
| `humidity` | float | Relative Luftfeuchte in % (`hum_01pct / 10`) |
| `lux` | uint16 | Beleuchtungsstärke in Lux |
| `auto_flags` | uint8 | Auto-Light-Bitmaske (siehe Tabelle unten) |
| `fault` | uint8 | Fehlercode (0 = kein Fehler) |
| `report_interval_s` | uint16 | Aktuelles Report-Intervall in Sekunden |
| `auto_on_lux_threshold` | uint16 | Aktuelle Einschaltschwelle in Lux |

### auto_flags Bitmaske

| Bit | Wert | Flag | Beschreibung |
|-----|------|------|-------------|
| 0 | 0x01 | AUTO_REQUEST_ON | Auto-Light-Einschaltvorgang läuft |
| 1 | 0x02 | AUTO_RELAY_OWNED | Relais wird von Automation gesteuert |
| 2 | 0x04 | BLOCKED_BY_SERVER | Server hat Auto-Light deaktiviert |
| 3 | 0x08 | BLOCKED_BY_LUX | Helligkeit zu hoch für Auto-Light |
| 4 | 0x10 | BLOCKED_BY_MISSING_LUX | Lux-Wert fehlt für Entscheidung |
| 5 | 0x20 | PRESENCE_SOURCE_AVAILABLE | Bewegungssensor vorhanden (PIR/LD2410) |
| 6 | 0x40 | LIGHT_VALUE_AVAILABLE | Luxsensor vorhanden (VEML7700) |
| 7 | 0x80 | LIGHT_GUARD_ENABLED | Auto-Light-Funktion aktiviert |

### 5.6 `smarthome/device/{device_id}/state` – NET-SEN State Payload (Wetterstation)

Wird bei jeder STATE-Nachricht eines NET-SEN gesendet.

```json
{
  "device_id": "NET-SEN-020",
  "temp_01c": 195,
  "hum_01pct": 620,
  "lux": 15000,
  "pressure_pa": 101325,
  "gas_ohm": null,
  "aqi": null,
  "tvoc_ppb": null,
  "eco2_ppm": null,
  "motion": false,
  "rain": false,
  "fault": false
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `temp_01c` | int16 | Temperatur in Zehntelgrad (°C × 10) |
| `hum_01pct` | uint16 | Relative Luftfeuchte in Zehntelprozent (% × 10) |
| `lux` | uint16 | Beleuchtungsstärke in Lux |
| `pressure_pa` | uint32 | Luftdruck in Pascal |
| `gas_ohm` | uint32 \| null | Gas-Sensor-Widerstand in Ohm (optional) |
| `aqi` | uint16 \| null | Luftqualitätsindex (optional) |
| `tvoc_ppb` | uint16 \| null | TVOC-Konzentration in ppb (optional) |
| `eco2_ppm` | uint16 \| null | eCO2-Konzentration in ppm (optional) |
| `motion` | bool | Präsenz erkannt |
| `rain` | bool \| null | Regenstatus, aus `rain_detected`-Events retained in den State übernommen |
| `fault` | bool | Fehlerstatus (true = Fehler) |

### 5.7 `smarthome/device/{device_id}/state` – BAT-SEN State Payload (Fensterkontakt)

Wird bei jeder STATE-Nachricht eines BAT-SEN im Fensterkontakt-Modus gesendet.

```json
{
  "device_id": "bat_sen_010",
  "battery_pct": 85,
  "battery_mv": 2900,
  "window_open": 0,
  "rain_raw": null,
  "button_flags": 0,
  "report_interval_s": 43200,
  "fault": false
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `battery_pct` | uint8 | Akkustand in Prozent |
| `battery_mv` | uint16 | Akkuspannung in Millivolt |
| `window_open` | uint8 | Fensterstatus (0 = geschlossen, >0 = geöffnet) |
| `rain_raw` | uint16 \| null | Regen-Rohwert (null wenn kein Regensensor) |
| `button_flags` | uint8 | Taster-Bitmaske |
| `report_interval_s` | uint16 | Meldeintervall in Sekunden |
| `fault` | bool | Fehlerstatus (true = Fehler) |

### 5.8 `smarthome/device/{device_id}/state` – BAT-SEN State Payload (Regensensor)

Wird bei jeder STATE-Nachricht eines BAT-SEN im Regensensor-Modus gesendet.

```json
{
  "device_id": "bat_sen_020",
  "battery_pct": 72,
  "battery_mv": 2600,
  "window_open": null,
  "rain_raw": 1845,
  "button_flags": 0,
  "report_interval_s": 900,
  "fault": false
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `device_id` | string | Geräte-ID |
| `battery_pct` | uint8 | Akkustand in Prozent |
| `battery_mv` | uint16 | Akkuspannung in Millivolt |
| `window_open` | uint8 \| null | Fensterstatus (null wenn kein Fensterkontakt) |
| `rain_raw` | uint16 | Regen-Rohwert (0 = kein Regen) |
| `button_flags` | uint8 | Taster-Bitmaske |
| `report_interval_s` | uint16 | Meldeintervall in Sekunden |
| `fault` | bool | Fehlerstatus (true = Fehler) |

### 5.9 Sensor-Offset (Temperatur/Feuchte)

Jedes Gerät unterstützt einen konfigurierbaren Offset zur Kompensation
von Einbaufehlern (z.B. Sensor zu nah am Netzteil):

| Gerät | temp_offset_01c | hum_offset_01pct |
|-------|:---------------:|:----------------:|
| NET-ERL-001 | 0 | 0 |
| NET-ERL-002 | 0 | 0 |
| NET-SEN-002 | 0 | 0 |

Offsets werden in Geräte-Nativeinheiten angegeben (Zehntelgrad, Zehntelprozent).
Negativer Wert = Korrektur nach unten. Positiver = Korrektur nach oben.
Beispiel: BME280 nahe Netzteil misst 30 °C bei 22 °C → temp_offset_01c = -80 (-8,0 °C).

Der Offset wird NACH der Sensor-Mittelung (EMA-Filter) angewendet.

### 5.10 Sensor-Mittelung (EMA-Filter)

Zur Rauschunterdrückung wird ein exponentiell gleitender Mittelwert (EMA)
auf Temperatur- und Feuchtewerte angewendet:

- α = 0,2 (20 % neuer Messwert, 80 % bisheriger Mittelwert)
- Aktiv in: NET-ERL-001 (BME280), NET-SEN-002 (BME280)
- ESPHome-Geräte: nutzen 16× Oversampling + IIR-Filter des BME280

---

## 6. Globale ESPHome-Internals

Folgende globale Variablen und Funktionen werden von den Contracts auf dem Master verwendet:

### 6.1 Globale Variablen

| Variable | Typ | Beschreibung |
|---|---|---|
| `contract_online` | `bool` | Master ist online und bereit |
| `contract_fault` | `bool` | Allgemeiner Fehlerstatus des Masters |
| `contract_last_status_code` | `int` | Letzter Status-Code |
| `contract_last_status_text` | `string` | Letzter Status-Text |
| `contract_last_request_id` | `int` | Letzte verarbeitete Request-ID |
| `stored_master_mac` | `string` | MAC-Adresse des Master-Gateways |
| `setup_portal_active` | `bool` | Setup-Portal ist aktiv (Captive Portal für Erstkonfiguration) |

### 6.2 Interne Funktionen

| Funktion | Beschreibung |
|---|---|
| **Setup-Portal** | Captive-Portal für WLAN-Konfiguration bei Erstinbetriebnahme |
| **OTA** | Over-the-Air-Update-Mechanismus (ESPHome standard) |
| **HELLO-Timeout** | Überwachung der Geräte-Erreichbarkeit, schaltet auf `"offline"` bei Ausbleiben von Nachrichten |

---

## 7. Nachrichtenfluss-Diagramme

### 7.1 Gerät anmelden (Boot)

```
Knoten              Master                MQTT-Server
  │                   │                      │
  │───HELLO──────────►│                      │
  │                   │──meta (retain)──────►│
  │                   │──availability ──────►│
  │◄──HELLO_ACK──────│                      │
  │                   │                      │
  │◄──CFG (optional)─│                      │
  │◄──CMD (optional)─│                      │
  │                   │                      │
```

### 7.2 Periodischer STATE (Cover)

```
Knoten              Master                MQTT-Server
  │                   │                      │
  │───STATE──────────►│                      │
  │                   │──state (retain)─────►│
  │◄──ACK────────────│                      │
  │                   │                      │
```

### 7.3 Befehl vom Server

```
Knoten              Master              MQTT-Server
  │                   │                      │
  │                   │◄──command───────────│
  │                   │                      │
  │◄──CMD────────────│                      │
  │                   │                      │
  │───ACK───────────►│                      │
  │                   │──ack───────────────►│
  │                   │                      │
  │───STATE─────────►│                      │
  │                   │──state (retain)─────►│
  │                   │                      │
```

### 7.4 Ereignisgesteuerter STATE

```
Knoten              Master                MQTT-Server
  │                   │                      │
  │───EVENT─────────►│                      │
  │                   │──state (retain)─────►│
  │◄──ACK────────────│                      │
  │                   │                      │
```

---

## 8. Events & Trigger

Events werden von Geräten über ESP-NOW gesendet (`msg_type = 0x04`) und vom Master in STATE-MQTT-Nachrichten übersetzt. Die Event-Typen dokumentieren, welche Zustandsänderungen ein Gerät melden kann.

### 8.1 Events

| Event | Wert | Beschreibung |
|---|---|---|
| `BUTTON_PRESS` | `0x01` | Taster gedrückt |
| `MOTION_DETECTED` | `0x02` | Bewegung erkannt |
| `WINDOW_OPENED` | `0x03` | Fenster geöffnet |
| `WINDOW_CLOSED` | `0x04` | Fenster geschlossen |
| `RAIN_DETECTED` | `0x05` | Regen erkannt |
| `RELAY_CHANGED` | `0x06` | Relais-Status geändert |
| `LIGHT_AUTO_ON` | `0x07` | Licht automatisch eingeschaltet |
| `LIGHT_AUTO_OFF` | `0x08` | Licht automatisch ausgeschaltet |
| `COVER_UP` | `0x09` | Cover fährt hoch |
| `COVER_DOWN` | `0x0A` | Cover fährt runter |
| `COVER_STOP` | `0x0B` | Cover gestoppt |
| `COVER_CALIB_START` | `0x0C` | Cover-Kalibrierung gestartet |
| `COVER_CALIB_DONE` | `0x0D` | Cover-Kalibrierung abgeschlossen |
| `NODE_BOOT` | `0x0E` | Gerät hochgefahren (nach Reset) |
| `SENSOR_FAULT` | `0x0F` | Sensorfehler aufgetreten |
| `COMM_FAULT` | `0x10` | Kommunikationsfehler |
| `BUTTON_RELEASE` | `0x11` | Taster losgelassen |
| `BUTTON_LONG_PRESS` | `0x12` | Taster lange gedrückt |

### 8.2 Trigger

| Trigger | Wert | Beschreibung |
|---|---|---|
| `MANUAL_BUTTON` | `0x01` | Manuell über Taster |
| `MASTER_CMD` | `0x02` | Vom Master gesendet |
| `AUTO` | `0x03` | Automatik (Regelung) |
| `AUTO_OFF_TIMER` | `0x04` | Automatische Abschaltung (Timer) |
| `CONFIG` | `0x05` | Konfigurationsänderung |

### 8.3 MQTT-Abbildung

Events werden nicht als eigenes MQTT-Topic publiziert, sondern lösen ein STATE-Update aus (§7.4). Das `event_type`- und `trigger`-Feld aus dem ESP-NOW `EventReportPayload` wird dabei in die entsprechende Zustandsänderung übersetzt (z.B. `cover_state`, `motion`, `relay_1`).

---

> **Änderungshistorie**  
> v1 – Initiale MQTT-Contract-Definition, Firmware Release 1.
