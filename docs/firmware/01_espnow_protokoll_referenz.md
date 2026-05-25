# ESP-NOW SmartHome Protokoll-Referenz

**Stand:** Firmware v1 · **Protokoll-Version:** 1 · **Magic Byte:** `0xA5`

---

## Inhaltsverzeichnis

1. [Überblick](#1-überblick)
2. [Paketaufbau (Layer 2)](#2-paketaufbau-layer-2)
3. [Header-Flags](#3-header-flags)
4. [Message-Typen](#4-message-typen)
5. [Payload-Strukturen](#5-payload-strukturen)
6. [Events & Trigger](#6-events--trigger)
7. [Command-Typen](#7-command-typen)
8. [Config-Parameter](#8-config-parameter)
9. [Cover-Commands](#9-cover-commands)
10. [Relay-Comfort-Flags](#10-relay-comfort-flags)
11. [ACK-Status-Codes](#11-ack-status-codes)
12. [Fehlercodes](#12-fehlercodes)
13. [Hilfsfunktionen](#13-hilfsfunktionen)
14. [Globale Konstanten](#14-globale-konstanten)

---

## 1. Überblick

Das SmartHome-Protokoll nutzt **ESP-NOW** als Transportlayer. Es ist ein binäres, verbindungsloses Protokoll mit optionaler ACK-Anforderung und CRC16-Sicherung.

| Eigenschaft | Wert |
|---|---|
| Magic Byte | `0xA5` |
| Protokoll-Version | `1` |
| Max. ESP-NOW Nutzlast | 250 Bytes |
| Header-Größe | 10 Bytes |
| Max. Payload-Größe | 240 Bytes |
| CRC | CRC16-CCITT (Poly `0x1021`) |
| Byte-Order | Little-Endian |

---

## 2. Paketaufbau (Layer 2)

Jedes ESP-NOW-Paket besteht aus einem **festen 10-Byte-Header** gefolgt von einem **variablen Payload** (0–240 Bytes).

### 2.1 MsgHeader (10 Bytes)

```
Offset  Größe  Feld            Beschreibung
──────────────────────────────────────────────────────
 0       1     magic           Magic Byte 0xA5
 1       1     proto_ver       Protokoll-Version (1)
  2       1     msg_type        Message-Typ (0x01–0x07, 0x09)
 3       1     seq             Sequenznummer (0–255)
 4       1     flags           Bit-Flags (siehe §3)
 5       1     _reserved       Reserviert (0)
 6       2     payload_len     Länge des Payload (Little-Endian)
 8       2     crc16           CRC16 über Header + Payload
──────────────────────────────────────────────────────
Total:   10 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  proto_ver;
    uint8_t  msg_type;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  _reserved;
    uint16_t payload_len;
    uint16_t crc16;
} MsgHeader;
```

### 2.2 Sequenznummern-System

Jedes Gerät führt einen eigenen 8-Bit-Sequenzzähler (`naechste_seq`, 0–255).
Bei jedem neuen ESP-NOW-Paket wird die aktuelle Sequenznummer verwendet und
der Zähler inkrementiert. Bei 255 erfolgt ein Überlauf zurück auf 0.

**Verwendung:**
- **ACK-Zuordnung**: Der Empfänger (Master) korreliert eingehende ACKs über
  die Sequenznummer mit dem ausstehenden Befehl (Pending-Slot).
- **Retry**: Bei Wiederholungen wird dieselbe Sequenznummer verwendet, damit
  der Empfänger Duplikate erkennen kann.
- **Duplikaterkennung**: Der Master ignoriert Pakete mit bereits bekannter
  Sequenznummer (kein Replay).

**Implementierung (sendePaketMitRetry):**
1. Aktuelle Sequenznummer sichern (`savedSeq = naechste_seq`)
2. Erstversand: sendPacket() → inkrementiert `naechste_seq` intern
3. Bei Misserfolg: `naechste_seq = savedSeq` (gleiche Seq für Retry)
4. Maximal NET_ERL_ESPNOW_RETRY_COUNT Wiederholungen

### 2.3 CRC16-Berechnung

Der CRC wird **über Header (ohne crc16-Feld) + Payload** gebildet:

```
CRC-Daten = magic[0] + proto_ver[1] + msg_type[2] + seq[3] + flags[4] + _reserved[5]
          + payload_len[6-7] + payload[0..payload_len-1]
```

**Algorithmus:** CRC16-CCITT mit Polynom `0x1021`, Startwert `0xFFFF`.

---

## 3. Header-Flags

| Flag | Wert | Beschreibung |
|---|---|---|
| `SH_FLAG_ACK_REQUEST` | `0x01` | Empfänger muss mit ACK antworten |
| `SH_FLAG_RETRANSMIT` | `0x02` | Paket ist eine Wiederholung |
| `SH_FLAG_ENCRYPTED` | `0x04` | Payload ist verschlüsselt |

Mehrere Flags können per OR kombiniert werden.

---

## 4. Message-Typen

| Typ | Wert | Richtung | Beschreibung |
|---|---|---|---|
| **HELLO** | `0x01` | → Master | Gerät meldet sich an |
| **HELLO_ACK** | `0x02` | Master → | Bestätigung der Anmeldung inkl. Kanal |
| **STATE** | `0x03` | → Master | Regelmäßiger Zustandsbericht |
| **EVENT** | `0x04` | → Master | Ereignismeldung (Taster, Bewegung, …) |
| **CMD** | `0x05` | Master → | Steuerbefehl (Relais, Cover, …) |
| **CFG** | `0x06` | Master → | Konfigurations-Parameter setzen |
| **ACK** | `0x07` | bidirektional | Bestätigung/Nicht-Bestätigung |
| **HEARTBEAT** | `0x09` | → Master | Lebenszeichen (bei batteriebetriebenen Knoten) |

### 4.1 Nachrichtenfluss – Anmeldung (HELLO/HELLO_ACK)

```
Knoten                    Master
  │                         │
  │──── HELLO ────────────→│
  │                         │ Prüft device_id, ordnet
  │                         │ Kanal zu, setzt config
  │←─── HELLO_ACK ─────────│
  │     (channel, status)   │
  │                         │
  │←─── CFG (optional) ────│
  │←─── CMD (optional) ────│
  │                         │
```

### 4.2 Nachrichtenfluss – Zustandsbericht (STATE)

```
Knoten                    Master
  │                         │
│──── STATE ────────────→│
│                         │
  │←─── ACK (wenn          │
  │     FLAG_ACK_REQUEST)   │
  │                         │
```

---

## 5. Payload-Strukturen

Alle Strukturen sind **`__attribute__((packed))`** (kein Padding).

### 5.1 HelloPayload (79 Bytes)

```
Offset  Größe  Feld                Beschreibung
─────────────────────────────────────────────────────────────
  0     16     device_id           Eindeutige Geräte-ID (z.B. "NET-ERL-001")
 16     32     device_name         Anzeigename (null-terminiert)
 48      1     device_class        Geräteklasse (§ DeviceTypes)
 49      1     caps_hi             Capabilities High-Byte
 50      1     caps_lo             Capabilities Low-Byte
 51      1     power_type          Stromversorgung (0 = MAINS, 1 = BATTERY)
 52      2     fw_version          Firmware-Version (major.minor)
 54      4     boot_counter        Hochlaufzähler
 58      1     meta_schema_version Meta-Schema-Version (1)
 59      1     control_mode        Steuerungsmodus
 60      1     config_profile      Konfigurationsprofil
 61      1     reporting_mode      Reporting-Modus
 62     11     sensor_mask         Bitmaske der Sensoren (11 Byte)
 73      6     input_mask          Bitmaske der Eingänge (6 Byte)
─────────────────────────────────────────────────────────────
Total:   79 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t  device_id[SH_DEVICE_ID_LEN];      // 16
    uint8_t  device_name[SH_DEVICE_NAME_LEN];  // 32
    uint8_t  device_class;
    uint8_t  caps_hi;
    uint8_t  caps_lo;
    uint8_t  power_type;
    uint16_t fw_version;
    uint32_t boot_counter;
    uint8_t  meta_schema_version;
    uint8_t  control_mode;
    uint8_t  config_profile;
    uint8_t  reporting_mode;
    uint8_t  sensor_mask[SH_SENSOR_MASK_LEN];  // 11
    uint8_t  input_mask[SH_INPUT_MASK_LEN];    // 6
} HelloPayload;
```

### 5.2 HelloAckPayload (4 Bytes)

```
Offset  Größe  Feld        Beschreibung
─────────────────────────────────────────
  0      1     channel     ESP-NOW Kanal (1–13)
  1      1     ack_status  Status der Anmeldung
  2      2     _pad        Padding (0)
─────────────────────────────────────────
Total:    4 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t channel;
    uint8_t ack_status;
    uint8_t _pad[2];
} HelloAckPayload;
```

### 5.3 HeartbeatPayload (20 Bytes)

```
Offset  Größe  Feld      Beschreibung
───────────────────────────────────────
  0     16     node_id   Geräte-ID
 16      4     uptime_s  Betriebssekunden
───────────────────────────────────────
Total:   20 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t node_id[SH_DEVICE_ID_LEN];  // 16
    uint32_t uptime_s;
} HeartbeatPayload;
```

### 5.4 CmdPayload (4 Bytes)

```
Offset  Größe  Feld      Beschreibung
───────────────────────────────────────
  0      1     cmd_type  Command-Typ (§8)
  1      1     param1    Parameter 1
  2      1     param2    Parameter 2
  3      1     _pad      Padding (0)
───────────────────────────────────────
Total:    4 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t cmd_type;
    uint8_t param1;
    uint8_t param2;
    uint8_t _pad;
} CmdPayload;
```

### 5.5 CfgPayload (4 Bytes)

```
Offset  Größe  Feld      Beschreibung
───────────────────────────────────────
  0      1     param_id  Config-Parameter-ID (§9)
  1      1     _pad      Padding (0)
  2      2     value     Parameter-Wert (Little-Endian)
───────────────────────────────────────
Total:    4 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t param_id;
    uint8_t _pad;
    uint16_t value;
} CfgPayload;
```

### 5.6 AckPayload (4 Bytes)

```
Offset  Größe  Feld          Beschreibung
───────────────────────────────────────────
  0      1     ack_seq       Sequenznummer des quittierten Pakets
  1      1     ack_msg_type  Message-Typ des quittierten Pakets
  2      1     status        Status-Code (§12)
  3      1     _pad          Padding (0)
───────────────────────────────────────────
Total:    4 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t ack_seq;
    uint8_t ack_msg_type;
    uint8_t status;
    uint8_t _pad;
} AckPayload;
```

### 5.7 State-Report-Payloads (Relay)

#### Basic: StateReportPayload (20 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];  // 16
    uint8_t  relay_1;                    // Relais-Status (0/1)
    uint8_t  fault;                      // Fehlercode (§13)
    uint8_t  _pad[2];
} StateReportPayload;                     // = 20 Bytes
```

#### Config: StateConfigReportPayload (22 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint8_t  _pad[2];
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} StateConfigReportPayload;               // = 22 Bytes
```

#### Comfort: RelayComfortStateReportPayload (27 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint8_t  _pad[2];
    int16_t  temp_01c;        // Temperatur in 1/100 °C
    uint16_t hum_01pct;       // Feuchte in 1/100 %
    uint16_t lux;             // Helligkeit in Lux
    uint8_t  motion;          // Bewegung (0/1)
    uint8_t  auto_flags;      // Comfort-Flags (§10)
    uint8_t  fault;
    uint8_t  _pad[1];
} RelayComfortStateReportPayload;          // = 27 Bytes
```

#### Extended Comfort: ExtendedRelayComfortStateReportPayload (37 Bytes)

```c
typedef struct __attribute__((packed)) {
    // ... Basis-Felder (16 + 1 + 1 + 2 + 2 + 2 + 2 + 1 + 1 + 1 + 1 = 30)
    uint32_t pressure_pa;     // Luftdruck in Pascal
    uint16_t aqi;             // Air Quality Index
    uint16_t tvoc_ppb;        // TVOC in ppb
    uint16_t eco2_ppm;        // eCO2 in ppm
} ExtendedRelayComfortStateReportPayload;  // = 37 Bytes
```

#### Extended Comfort + Gas: ExtendedRelayComfortGasStateReportPayload (41 Bytes)

```c
// Wie ExtendedRelayComfortStateReportPayload, aber pressure_pa ersetzt durch gas_ohm
typedef struct __attribute__((packed)) {
    // ... Basis-Felder (27)
    uint32_t gas_ohm;         // Gas-Sensor-Widerstand in Ohm
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
} ExtendedRelayComfortGasStateReportPayload; // = 41 Bytes
```

### 5.8 State-Report-Payloads (Dual Relay / Cover)

#### ZrlStateReportPayload (23 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t node_id[SH_DEVICE_ID_LEN];
    uint8_t relay_1;
    uint8_t relay_2;
    uint8_t cover_mode;
    uint8_t cover_state;      // 0=stopped, 1=up, 2=down
    uint8_t cover_position;   // 0–100 (255 = nicht kalibriert)
    uint8_t cover_calibrated; // 0/1
    uint8_t fault;
} ZrlStateReportPayload;                    // = 23 Bytes
```

> **Hinweis:** `cover_mode` enthält das zuletzt ausgeführte Cover-Kommando (`SH_COVER_CMD_*`), nicht einen persistenten Modus-Flag.

#### ZrlConfigStateReportPayload (25 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  relay_2;
    uint8_t  cover_mode;
    uint8_t  cover_state;
    uint8_t  cover_position;
    uint8_t  cover_calibrated;
    uint8_t  fault;
    uint16_t report_interval_s;
} ZrlConfigStateReportPayload;              // = 25 Bytes
```

### 5.9 State-Report-Payloads (Sensor)

#### SensorStateReportPayload (24 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
} SensorStateReportPayload;                 // = 24 Bytes
```

#### SensorConfigStateReportPayload (26 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
    uint16_t report_interval_s;
} SensorConfigStateReportPayload;           // = 26 Bytes
```

#### ExtendedSensorStateReportPayload (34 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
    uint32_t pressure_pa;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
} ExtendedSensorStateReportPayload;         // = 34 Bytes
```

#### ExtendedSensorGasStateReportPayload (38 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
    uint32_t gas_ohm;       // Gas-Sensor in Ohm
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
} ExtendedSensorGasStateReportPayload;      // = 38 Bytes
```

### 5.10 State-Report-Payloads (Battery)

#### BatteryStateReportPayload (24 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    uint8_t  window_open;
    uint16_t rain_raw;
    uint8_t  button_flags;
    uint8_t  fault;
} BatteryStateReportPayload;                // = 24 Bytes
```

#### BatteryConfigStateReportPayload (26 Bytes)

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    uint8_t  window_open;
    uint16_t rain_raw;
    uint8_t  button_flags;
    uint8_t  fault;
    uint16_t report_interval_s;
} BatteryConfigStateReportPayload;          // = 26 Bytes
```

### 5.11 EventReportPayload (22 Bytes)

```
Offset  Größe  Feld        Beschreibung
───────────────────────────────────────────
  0     16     node_id     Geräte-ID
 16      1     event_type  Event-Typ (§7)
 17      1     trigger     Auslöser (§7)
 18      1     param1      Parameter 1 (z.B. Button-Index)
 19      2     param2      Parameter 2 (z.B. Relais-Zustand)
 21      1     _pad        Padding (0)
───────────────────────────────────────────
Total:   22 Bytes
```

```c
typedef struct __attribute__((packed)) {
    uint8_t  node_id[SH_DEVICE_ID_LEN];
    uint8_t  event_type;
    uint8_t  trigger;
    uint8_t  param1;
    uint16_t param2;
    uint8_t  _pad;
} EventReportPayload;                       // = 22 Bytes
```

---

## 6. Events & Trigger

### 6.1 Events

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

### 6.2 Trigger

| Trigger | Wert | Beschreibung |
|---|---|---|
| `UNKNOWN` | `0x00` | Unbekannt / nicht gesetzt |
| `MANUAL_BUTTON` | `0x01` | Manuell über Taster |
| `MASTER_CMD` | `0x02` | Vom Master gesendet |
| `AUTO` | `0x03` | Automatik (Regelung) |
| `AUTO_OFF_TIMER` | `0x04` | Automatische Abschaltung (Timer) |
| `CONFIG` | `0x05` | Konfigurationsänderung |

---

## 7. Command-Typen

| Command | Wert | param1 | param2 | Beschreibung |
|---|---|---|---|---|
| `SH_CMD_RELAY` | `0x01` | Relais-Index (0/1) | 0=aus, 1=an | Relais schalten |
| `SH_CMD_COVER` | `0x02` | Cover-Command (§10) | Position (0–100) | Cover steuern |
| `SH_CMD_STATE_REQUEST` | `0x03` | — | — | Sofortigen STATE anfordern |
| `SH_CMD_REBOOT` | `0x04` | — | — | Neustart auslösen |
| `SH_CMD_SET_RELAY` | `0x01` | (Alias für SH_CMD_RELAY) | | |
| `SH_CMD_HELLO_REQUEST` | `0x06` | — | — | HELLO erneut senden |

---

## 8. Config-Parameter

### 8.1 Basis-Parameter

| ID | Name | Typ | Standard | Beschreibung |
|---|---|---|---|---|
| `0x01` | `SH_CFG_DEVICE_NAME` | String(32) | — | Anzeigename |
| `0x02` | `SH_CFG_REPORT_INTERVAL_S` | `uint16_t` | 60 s | STATE-Sendeintervall |
| `0x03` | `SH_CFG_ACK_TIMEOUT_MS` | `uint16_t` | 100 ms | Wartezeit auf ACK |
| `0x04` | `SH_CFG_MAX_RETRIES` | `uint16_t` | 3 | Maximale Wiederholungen |
| `0x05` | `SH_CFG_EVENT_DEBOUNCE_MS` | `uint16_t` | 50 ms | Entprellzeit |
| `0x06` | `SH_CFG_LED_ENABLED` | `uint16_t` | 1 | LED aktiv (0/1) |

### 8.2 Sensor-Parameter

| ID | Name | Typ | Beschreibung |
|---|---|---|---|
| `0x10` | `SH_CFG_TEMP_DELTA_01C` | `uint16_t` | Temperatur-Änderungsschwelle in 1/100 °C |
| `0x11` | `SH_CFG_HUM_DELTA_01PCT` | `uint16_t` | Feuchte-Änderungsschwelle in 1/100 % |
| `0x12` | `SH_CFG_LUX_DELTA` | `uint16_t` | Helligkeits-Änderungsschwelle in Lux |
| `0x13` | `SH_CFG_PRESENCE_HOLD_S` | `uint16_t` | Präsenz-Nachhaltezeit in Sekunden |

### 8.3 Relay-Parameter

| ID | Name | Typ | Beschreibung |
|---|---|---|---|
| `0x20` | `SH_CFG_RELAY_MODE` | `uint16_t` | Relais-Modus (z.B. Schalten/Dimmen) |
| `0x21` | `SH_CFG_AUTO_OFF_DELAY_S` | `uint16_t` | Automatische Ausschaltverzögerung (s) |
| `0x22` | `SH_CFG_LIGHT_THRESHOLD_ON` | `uint16_t` | Heller-Schwelle für Lichtautomatik (lux) |
| `0x23` | `SH_CFG_LIGHT_THRESHOLD_OFF` | `uint16_t` | Dunkel-Schwelle für Lichtautomatik (lux) |
| `0x24` | `SH_CFG_RELAY_DEFAULT_ON_BOOT` | `uint16_t` | Relais-Standard nach Boot (0/1) |
| `0x25` | `SH_CFG_AUTOMATION_ENABLED` | `uint16_t` | Automatik aktiv (0/1) |

### 8.4 Cover-Parameter

| ID | Name | Typ | Beschreibung |
|---|---|---|---|
| `0x30` | `SH_CFG_COVER_RUN_UP_MS` | `uint16_t` | Fahrzeit hoch in ms |
| `0x31` | `SH_CFG_COVER_RUN_DOWN_MS` | `uint16_t` | Fahrzeit runter in ms |
| `0x32` | `SH_CFG_COVER_REVERSE_LOCK_MS` | `uint16_t` | Sperrzeit bei Richtungswechsel (ms) |
| `0x33` | `SH_CFG_COVER_CALIBRATED` | `uint16_t` | Kalibrierungsstatus (0/1) |

### 8.5 Battery-Parameter

| ID | Name | Typ | Beschreibung |
|---|---|---|---|
| `0x40` | `SH_CFG_WAKE_INTERVAL_S` | `uint16_t` | Aufwach-Intervall (s) |
| `0x41` | `SH_CFG_RX_WINDOW_MS` | `uint16_t` | Empfangsfenster nach Aufwachen (ms) |
| `0x42` | `SH_CFG_LOW_BATTERY_PCT` | `uint16_t` | Batterie-warnschwelle (%) |

### 8.6 LED-Ring-Parameter

| ID | Name | Typ | Beschreibung |
|---|---|---|---|
| `0x50` | `SH_CFG_RING_ENABLED` | `uint16_t` | LED-Ring aktiv (0/1) |
| `0x51` | `SH_CFG_RING_BRIGHTNESS` | `uint16_t` | Helligkeit (0–255) |
| `0x52` | `SH_CFG_RING_MODE` | `uint16_t` | Betriebsmodus des Rings |

---

## 9. Cover-Commands

| Command | Wert | Beschreibung |
|---|---|---|
| `SH_COVER_CMD_OPEN` | `0x01` | Cover vollständig öffnen |
| `SH_COVER_CMD_CLOSE` | `0x02` | Cover vollständig schließen |
| `SH_COVER_CMD_STOP` | `0x03` | Bewegung sofort stoppen |
| `SH_COVER_CMD_SET_POSITION` | `0x04` | Auf bestimmte Position fahren (param2 = 0–100 %) |

### Cover-States

| State | Wert | Beschreibung |
|---|---|---|
| `STOPPED` | `0x00` | Cover steht still |
| `MOVING_UP` | `0x01` | Cover fährt hoch |
| `MOVING_DOWN` | `0x02` | Cover fährt runter |

---

## 10. Relay-Comfort-Flags

Die Comfort-Automatik-Flags werden als Bitmaske im `auto_flags`-Feld der Comfort-State-Report-Payloads gesendet.

| Flag | Wert | Beschreibung |
|---|---|---|
| `AUTO_REQUEST_ON` | `0x01` | Automatik fordert Einschalten |
| `AUTO_RELAY_OWNED` | `0x02` | Relais wird von der Automatik verwaltet |
| `BLOCKED_BY_SERVER` | `0x04` | Vom Server blockiert (Kindersicherung) |
| `BLOCKED_BY_LUX` | `0x08` | Wegen Helligkeit blockiert |
| `BLOCKED_BY_MISSING_LUX` | `0x10` | Wegen fehlendem Lux-Sensor blockiert |
| `PRESENCE_SOURCE_AVAILABLE` | `0x20` | Bewegungsmelder verfügbar |
| `LIGHT_VALUE_AVAILABLE` | `0x40` | Helligkeitswert verfügbar |
| `LIGHT_GUARD_ENABLED` | `0x80` | Licht-Überwachung aktiv |

---

## 11. ACK-Status-Codes

| Code | Wert | Beschreibung |
|---|---|---|
| `SH_ACK_OK` | `0x00` | Erfolgreich verarbeitet |
| `SH_ACK_ERROR` | `0x01` | Allgemeiner Fehler |
| `SH_ACK_REJECTED` | `0x02` | Abgelehnt (z.B. unbekannte device_id) |
| `SH_ACK_REJECTED_FULL` | `0x03` | Abgelehnt – Warteschlange voll |

### ACK-Retransmission (Timeout/Retry)

Wenn ein Kommando mit `SH_FLAG_ACK_REQUEST` gesendet wird, erwartet der Sender
eine ACK-Antwort. Der Ablauf:

1. **Senden**: ESP-NOW-Paket mit ACK-Request-Flag
2. **Pending-Slot**: Kommando wird als "ausstehend" markiert
   - `letztes_senden_ms = millis()`
   - `retries = 0`
3. **Warten**: `COMMAND_ACK_TIMEOUT_MS` (800 ms) auf ACK
4. **ACK empfangen**: Sequenznummer prüfen → Pending-Slot löschen → ACK an Server
5. **Timeout**: 
   - Wenn `retries < COMMAND_MAX_RETRIES` (2): erneut senden mit gleicher Seq
   - Wenn `retries >= COMMAND_MAX_RETRIES`: Fehler-ACK an Server (`status: "timeout"`)
6. **Abbruch**: Wenn Sender-MAC unbekannt → `status: "no_route"` ohne Retry

Der Timeout-Check erfolgt in der Hauptschleife über `pruefePendingTimeoutsImpl<T>()`,
die sowohl CMD- als auch CFG-Pending-Slots mit derselben Logik behandelt.

---

## 12. Fehlercodes

| Code | Wert | Beschreibung |
|---|---|---|
| `NONE` | `0x00` | Kein Fehler |
| `SENSOR_INIT` | `0x01` | Sensor-Initialisierung fehlgeschlagen |
| `SENSOR_READ` | `0x02` | Sensor-Lesen fehlgeschlagen |
| `ACK_TIMEOUT` | `0x03` | ACK-Timeout (Kommunikation) |
| `COVER_CALIB` | `0x04` | Cover-Kalibrierungsfehler |

---

## 13. Hilfsfunktionen

| Funktion | Beschreibung |
|---|---|
| `fillHeader(header, msg_type, seq, flags, payload_len)` | Setzt magic, version, msg_type, seq, flags, payload_len und berechnet CRC16. |
| `isValidHeader(header)` | Prüft magic == 0xA5, version == 1, payload_len <= SH_MAX_PAYLOAD_BYTES. |
| `safeCopyDeviceId(src, dst)` | Kopiert device_id und wandelt in Großbuchstaben um, erlaubt nur A-Z, 0-9, Bindestrich. |
| `safeCopyMask(src, dst, dst_len, fill_char)` | Kopiert eine Maske mit Validierung und Auffüllung. |
| `isValidDeviceId(id)` | Prüft Format wie `"NET-ERL-001"`: 10–11 Zeichen, Großbuchstaben/Ziffern/Bindestrich, endet mit 3 Ziffern. |
| `isValidMac(mac)` | Prüft dass MAC nicht all-zero oder all-FF ist. |
| `macToString(mac, buffer)` | Formatiert MAC als `"XX:XX:XX:XX:XX:XX"`. |
| `calcCrc16(data, len)` | Berechnet CRC16-CCITT (Poly `0x1021`). |
| `calcPacketCrc(header, payload)` | CRC über gesamtes Paket (Header ohne crc16 + Payload). |
| `finalizePacketCrc(header, payload)` | Setzt `header.crc16` auf den korrekten CRC. |
| `hasValidPacketCrc(packet, len)` | Validiert den CRC eines empfangenen Pakets. |

---

## 14. Globale Konstanten

| Konstante | Wert | Beschreibung |
|---|---|---|
| `SH_PROTO_MAGIC` | `0xA5` | Magic Byte |
| `SH_PROTO_VERSION` | `1` | Protokoll-Version |
| `SH_ESPNOW_MAX_BYTES` | `250` | Maximale ESP-NOW Nutzlast |
| `SH_HEADER_SIZE` | `10` | Header-Größe |
| `SH_MAX_PAYLOAD_BYTES` | `240` | Maximale Payload-Größe |
| `SH_DEVICE_ID_LEN` | `16` | Maximale Länge der Geräte-ID |
| `SH_DEVICE_NAME_LEN` | `32` | Maximale Länge des Gerätenamens |
| `SH_SENSOR_MASK_LEN` | `11` | Länge der Sensor-Bitmaske |
| `SH_INPUT_MASK_LEN` | `6` | Länge der Eingangs-Bitmaske |
| `SH_MAX_DEVICES` | `32` | Maximale Anzahl verwalteter Geräte |

---

> **Änderungshistorie**  
> v1 – Initiale Protokoll-Definition, Firmware Release 1.
