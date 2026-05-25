# NET-ERL-001 — Hall Module

**Device ID:** `NET-ERL-001`  
**Device Name:** `NET-ERL Hall Module`  
**FW-Variante:** `net_erl_hall_module`  
**Config-Profil:** `HALL_LIGHT` (`SH_PROFILE_HALL_LIGHT`)  
**Steuerungsmodus:** `RELAY_LIGHT` (`SH_CONTROL_MODE_RELAY_LIGHT`)  
**Reporting-Modus:** `HYBRID` (periodisch + ereignisgesteuert)  
**Geräteklasse:** `SH_CLASS_NET_ERL` (0x01)  
**Stromversorgung:** `SH_POWER_MAINS` (Netzbetrieben)

---

## 1. Übersicht

Das NET-ERL-001 Hall Module ist ein netzbetriebener Relais-Komfortaktor für den Flur-/Hausflurbereich. Es kombiniert einen PIR-Bewegungssensor mit einer automatischen Lichtsteuerung (Auto-Light) auf Basis der Umgebungshelligkeit (Lux). Ein BME280 liefert Temperatur und Luftfeuchte, ein VEML7700 die Beleuchtungsstärke. Das Gerät kommuniziert per ESP-NOW mit einem Master und meldet Zustandsänderungen sowohl periodisch als auch ereignisgesteuert (HYBRID-Modus).

### Fähigkeiten (Capabilities)

| Capability | Bit | Beschreibung |
|---|---|---|
| `SH_CAP_RELAY` | 0x0001 | Ein Relaisausgang |
| `SH_CAP_TEMP` | 0x0004 | Temperatursensor (BME280) |
| `SH_CAP_HUM` | 0x0008 | Luftfeuchtesensor (BME280) |
| `SH_CAP_LUX` | 0x0010 | Helligkeitssensor (VEML7700) |
| `SH_CAP_MOTION` | 0x0040 | PIR-Bewegungssensor |

---

## 2. Hardware-Pin-Belegung

| Funktion | GPIO | Typ | Beschreibung |
|---|---|---|---|
| Relais 1 | 10 | OUTPUT, active-HIGH | Schaltausgang für Beleuchtung |
| PIR-Sensor | 7 | INPUT | HIGH = Bewegung erkannt |
| I2C SDA | 0 | I2C | BME280 + VEML7700 Datenleitung |
| I2C SCL | 1 | I2C | BME280 + VEML7700 Taktleitung |
| Setup-Button | 2 | INPUT, active-LOW | 5000 ms Haltezeit für Setup-Modus |
| Setup-Indikator-LED | 6 | OUTPUT, active-HIGH | 500 ms Blinkintervall im Setup-Modus |
| Status-LED | -1 | — | Nicht bestückt |

**Relais-Polarität:** `RELAY_1_ACTIVE_HIGH = 1` — HIGH (3,3 V) schaltet das Relais ein.

### Setup-Taster (GPIO 2)

- **5 Sekunden halten**: Aktiviert den Setup-Modus
- **Setup-Modus**: Das Gerät startet einen WiFi-Access-Point (SSID = Geräte-ID).
  Über das Webinterface können Master-MAC und Konfiguration eingestellt werden.
- **Normale Funktion**: Während des Setup-Modus pausiert die Sensorik und
  ESP-NOW-Kommunikation.
- **Verlassen**: Nach Speichern der Konfiguration oder Timeout (5 Minuten)
  wird der Setup-Modus beendet und das Gerät startet neu.

Die Haltezeit von 5 Sekunden verhindert versehentliches Aktivieren.
Während des Haltens blinkt die Status-LED schnell (200 ms Intervall).

---

## 3. Timing-Parameter

| Parameter | Wert | Beschreibung |
|---|---|---|
| HELLO-Retry-Intervall | 5000 ms | Wiederholung der HELLO-Nachricht bei Master-Suche |
| Heartbeat-Intervall | 20000 ms | Periodisches Lebenszeichen |
| Loop-Intervall | 20 ms | Hauptschleifen-Pause |
| Sensor-Poll-Intervall (PIR) | 250 ms | Abfrageintervall des PIR-Sensors |
| Env-Sample-Intervall | 60000 ms (60 s) | Abstand zwischen Umweltsensor-Messungen |
| Recovery-Retry-Intervall | 30000 ms (30 s) | Wiederholungsintervall für defekte Sensoren |
| Default-Report-Intervall | 10 s | Standard-Zustandsreport-Intervall |
| Min/Max Report-Intervall | 5 s / 600 s | Konfigurierbare Grenzen |
| Auto-Off-Delay | 15 s (default) | Nachlaufzeit nach letzter Bewegung |
| Auto-On-Lux-Schwelle | 250 Lux (default) | Helligkeitsschwelle für automatisches Einschalten |

### Sensor-Konfiguration

**BME280:**
- I2C-Adresse: `0x76`
- Sensor-Masken-Position: `T` (Temperatur) und `H` (Feuchte)
- Der Luftdruck (Pressure) wird vom BME280 nicht als Capability gemeldet (bewusster Design-Entscheid — bleibt außerhalb des externen Capability-Vertrags)

**VEML7700:**
- I2C-Adresse: Standard (0x10)
- Gain: `VEML7700_GAIN_1`
- Integrationszeit: `VEML7700_IT_100MS`

---

## 4. Runtime-Konstanten

| Define | Wert | Beschreibung |
|---|---|---|
| `NET_ERL_STORAGE_NS` | `net_erl_hl` | NVS-Namespace für persistente Daten |
| `NET_ERL_SENSOR_MASK` | `THLMXXXXXX` | Sensor-Maske (T=Temp, H=Hum, L=Lux, M=Motion) |
| `NET_ERL_INPUT_MASK` | `XXXXX` | Input-Maske (keine Eingabegeräte) |
| `NET_ERL_PERSISTED_MAGIC` | `0x484C4C31` | Magic-Number für NVS-Persistenz |
| `NET_ERL_PERSISTED_KEY` | `hall_setup_v1` | NVS-Key für Konfiguration |
| `NET_ERL_USE_ISR_CMD_QUEUE` | 1 | CMD-Queue aus Interrupt-Kontext (ISR-sicher) |
| `NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION` | 1 | Neue PIR-Erkennung setzt Nachlauf-Timer zurück |
| `NET_ERL_WDT_TIMEOUT_S` | 8 s | Task-Watchdog-Timeout |

---

## 5. ESP-NOW-Protokoll und Payload-Mapping

### 5.1 HELLO-Payload (79 Bytes)

Gesendet beim Boot und periodisch bis zum HELLO-ACK.

| Feld | Typ | Bytes | Wert |
|---|---|---|---|
| `device_id` | char[16] | 16 | `NET-ERL-001` |
| `device_name` | char[32] | 32 | `NET-ERL Hall Module` |
| `device_class` | uint8_t | 1 | `SH_CLASS_NET_ERL` (0x01) |
| `caps_hi` | uint8_t | 1 | `(DEVICE_CAPS >> 8) & 0xFF` |
| `caps_lo` | uint8_t | 1 | `DEVICE_CAPS & 0xFF` → Temp/Hum/Lux/Motion/Relay |
| `power_type` | uint8_t | 1 | `SH_POWER_MAINS` (0x00) |
| `fw_version` | uint16_t | 2 | Aktuelle Firmware-Version |
| `boot_counter` | uint32_t | 4 | Boot-Zähler (inkrementiert bei Neustart) |
| `meta_schema_version` | uint8_t | 1 | `SH_META_SCHEMA_VERSION_CURRENT` |
| `control_mode` | uint8_t | 1 | `SH_CONTROL_MODE_RELAY_LIGHT` |
| `config_profile` | uint8_t | 1 | `SH_PROFILE_HALL_LIGHT` (0x01) |
| `reporting_mode` | uint8_t | 1 | `SH_REPORTING_HYBRID` |
| `sensor_mask` | char[11] | 11 | `THLMXXXXXX` |
| `input_mask` | char[6] | 6 | `XXXXX` |

### 5.2 STATE-Payload: RelayComfortConfigStateReportPayload (31 Bytes)

Genutzt für periodische Zustandsmeldungen und als Antwort auf `SH_CMD_STATE_REQUEST`.

```
Offset  Größe  Feld                  Typ      Beschreibung
------  -----  --------------------  -------  -----------------------------------------
 0      16     node_id               char[]   Geräte-ID "NET-ERL-001"
16       1     relay_1               uint8_t  Relais-Zustand (0=AUS, 1=EIN)
17       2     temp_01c              int16_t  Temperatur in 1/10 °C (z.B. 235 = 23,5 °C)
19       2     hum_01pct             uint16_t Relative Feuchte in 0,1 % (z.B. 455 = 45,5 %)
21       2     lux                   uint16_t Beleuchtungsstärke in Lux
23       1     motion                uint8_t  Bewegung (0=keine, 1=erkannt)
24       1     auto_flags            uint8_t  Bitmaske (siehe 5.4)
25       1     fault                 uint8_t  Fehlerstatus (0=ok, >0=Fehler)
26       1     _pad                  uint8_t  Padding
27       2     report_interval_s     uint16_t Aktuelles Report-Intervall (Sekunden)
29       2     auto_on_lux_threshold uint16_t Aktuelle Einschaltschwelle (Lux)
```

### 5.3 EVENT-Payload: EventReportPayload (22 Bytes)

Gesendet bei:
- Bewegung erkannt/verloren (`SH_EVENT_MOTION_DETECTED`, Trigger: `SH_TRIGGER_AUTO`)
- Relais-Schaltung durch Auto-Light (`SH_EVENT_RELAY_CHANGED`, Trigger: `SH_TRIGGER_AUTO` / `SH_TRIGGER_AUTO_OFF_TIMER`)

### 5.4 Auto-Flags Bitmaske

| Bit | Flag | Beschreibung |
|---|---|---|
| 0 | `SH_RELAY_COMFORT_FLAG_AUTO_REQUEST_ON` | Auto-On-Entscheidung steht aus |
| 1 | `SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED` | Relais wird von Auto-Light gesteuert |
| 2 | `SH_RELAY_COMFORT_FLAG_BLOCKED_BY_SERVER` | (nicht verwendet) |
| 3 | `SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX` | Auto-On wegen zu hoher Helligkeit blockiert |
| 4 | `0x10` (Hall-spezifisch) | Pending-Entscheidung, aber Lux-Wert fehlt |
| 5 | `SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE` | PIR meldet Bewegung |
| 6 | `SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE` | VEML7700 ist verfügbar |
| 7 | `SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED` | Luxschutz ist immer aktiv |

---

## 6. MQTT-Topics (abgeleitet)

Der Master übersetzt ESP-NOW-Nachrichten in MQTT. Abgeleitete Topics:

```
smarthome/<device_id>/state        → STATE-Payload (31 Bytes hex/binary)
smarthome/<device_id>/event        → EVENT-Payload (22 Bytes hex/binary)
smarthome/<device_id>/cmd          → Master sendet CMD (4 Bytes)
smarthome/<device_id>/cfg          → Master sendet CFG (4 Bytes)
smarthome/<device_id>/hello        → HELLO-Payload (79 Bytes hex/binary)
smarthome/<device_id>/heartbeat    → HEARTBEAT-Payload (20 Bytes hex/binary)
smarthome/<device_id>/available    → Online/Offline (last will / heartbeat)
```

---

## 7. Auto-Light-Zustandsautomat

Der Auto-Light-Mechanismus realisiert eine bewegungsgesteuerte Lichtsteuerung mit Helligkeitsschutz.

### Zustände

```
          ┌──────────────────────────────────────────────┐
          │                                              │
          ▼                                              │
    ┌──────────┐    PIR=HIGH, Lux≤Schwelle     ┌──────────────┐
    │  IDLE    │ ──────────────────────────────▶│ RELAY_AUTO   │
    │ Relais   │    oder Late-Lux entscheidet    │ Relais EIN   │
    │ AUS      │                                 │ relay_auto   │
    │ kein     │                                 │ _owned=true  │
    │ Motion   │                                 │              │
    └──────────┘                                 └──────────────┘
        ▲                                              │
        │                                              │
        │    PIR=LOW, Nachlauf abgelaufen              │
        │    (auto_off_delay_s Sekunden)               │
        │    und relay_auto_owned                      │
        └──────────────────────────────────────────────┘

    ┌──────────────┐
    │ RELAY_MANUAL │ ← Master-CMD oder Button toggelt Relais
    │ Relais EIN   │   (relay_auto_owned=false)
    │ manuell      │
    │ + Motion-    │
    │   Folgelogik │
    └──────────────┘
```

### Ablauf im Detail

1. **PIR-HIGH erkannt:**
   - `motion_aktiv = true`, `letzte_motion_ms = nowMs`
   - Wenn Relais AUS → `pending_auto_on_decision = true`
   - **Sofortige Entscheidung:** `tryAutoOnFromCachedLux()` prüft den letzten bekannten Luxwert
     - Lux ≤ Schwelle (250): Relais EIN (`relay_auto_owned = true`), AUTO-Event senden
     - Lux > Schwelle: `blocked_by_lux = true`, Relais bleibt AUS
     - Kein Luxwert verfügbar → bleibt `pending_auto_on_decision = true` für Late-Lux

2. **Late-Lux-Logik (im Sensor-Poll):**
   - Wenn `pending_auto_on_decision` und `lux != 0xFFFF`:
     - Lux ≤ Schwelle → Relais EIN, AUTO-Event
     - Lux > Schwelle → `blocked_by_lux = true`

3. **PIR-LOW (keine Bewegung):**
   - Nachlauf-Timer läuft (`auto_off_delay_s` Sekunden)
   - `NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION = 1`: Jede erneute PIR-Erkennung setzt den Timer zurück
   - Nach Ablauf: `motion_aktiv = false`
     - Wenn `relay_auto_owned` → Relais AUS, AUTO_OFF_TIMER-Event
     - Wenn `manual_follow_motion_active` und `manual_follow_motion_seen` → Relais AUS

4. **Manuelle Übersteuerung:**
   - Master-CMD (`SH_CMD_SET_RELAY`): Relais wird gesetzt, `relay_auto_owned = false`, `pending_auto_on_decision = false`
   - Button-Short-Press: Relais toggelt, danach Motion-Folgelogik aktiv
   - Manuelles AUS → `holdExplicitOffUntilNextMotionRisingEdge()` verhindert sofortiges Wiedereinschalten

### Delta-Detektion

STATE-Reports werden nur bei relevanten Änderungen gesendet:

| Sensor | Delta-Bedingung |
|---|---|
| Temperatur | Jede Änderung (ungleich letztem Wert) |
| Luftfeuchte | Änderung ≥ 5 (0,5 % rF) |
| Lux | Änderung ≥ 5 Lux |

---

## 8. Device-Hook-Implementierungen

### `netErlDeviceInit()`
Initialisiert I2C-Bus (SDA=GPIO0, SCL=GPIO1), BME280 an Adresse 0x76, VEML7700 mit 100 ms Integrationszeit, PIR-Pin (GPIO7) als INPUT und Status-LED falls bestückt.

### `netErlDeviceResetSensorDefaults()`
Setzt `temp_01c = INT16_MIN`, `hum_01pct = 0xFFFF`, `lux = 0xFFFF` als ungültige Markierungen.

### `netErlDeviceReadPresence()`
Liest PIR-Pin (GPIO7), gibt `true` bei HIGH zurück.

### `netErlDeviceSetRelayOutput(bool on)`
Setzt Relais-Pin (GPIO10) entsprechend `RELAY_1_ACTIVE_HIGH`. Setzt optional die Status-LED.

### `netErlDevicePollSensors(unsigned long nowMs)`
Haupt-Sensor-Poll mit 60 s Rate-Limit:
1. **Recovery:** Ausgefallene Sensoren werden alle 30 s neu initialisiert
2. **BME280:** Liest Temperatur (°C) und Feuchte (%), konvertiert in 1/10 °C und 0,1 % rF, Plausibilitätsprüfung (0–100 % rF)
3. **VEML7700:** Liest Lux, clamps auf uint16_t
4. **Late-Lux:** Entscheidet ausstehendes Auto-On mit erstem gültigen Luxwert
5. **Delta-Detektion:** Markiert `state_report_offen` bei relevanten Änderungen

### EMA-Filter (Rauschunterdrückung)

Temperatur und Feuchte werden über einen exponentiell gleitenden Mittelwert
geglättet, um kurzfristige Messschwankungen zu unterdrücken:

- **α = 0,2**: 20 % neuer Messwert, 80 % bisheriger Mittelwert
- **Erste Messung**: initialisiert den Filter direkt (keine Glättung)
- **Sensor-Reset**: Filter wird zurückgesetzt (NAN), beginnt neu

Der EMA-Filter arbeitet im Hundertstel-Bereich (temp_01c) für maximale Präzision.
Erst NACH der Filterung wird der konfigurierte Offset (TEMP_OFFSET_01C) addiert.

### Sensor-Offset

Zur Kompensation von Einbaufehlern (z.B. Sensor nahe Netzteil) unterstützt
das Gerät einen konfigurierbaren Offset:

- `NET_ERL_TEMP_OFFSET_01C`: Temperatur-Korrektur in Zehntelgrad
- `NET_ERL_HUM_OFFSET_01PCT`: Feuchte-Korrektur in Zehntelprozent

Negativer Wert = nach unten korrigieren. Beispiel: BME280 nahe Netzteil misst
30 °C bei 22 °C Raumtemperatur → Temp-Offset = -80 (-8,0 °C).

Der Offset wird NACH dem EMA-Filter, aber VOR der Delta-Detection angewendet.

### `netErlDeviceFillStatePayload(void* payload, size_t* size)`
Befüllt `RelayComfortConfigStateReportPayload` (31 Bytes) mit aktuellen Messwerten:
- `node_id`, `relay_1`, `temp_01c`, `hum_01pct`, `lux`, `motion`, `auto_flags`, `fault`, `report_interval_s`, `auto_on_lux_threshold`

### `netErlDeviceBuildAutoFlags()`
Baut 8-Bit-Bitmaske aus Runtime-Flags:
- `PRESENCE_SOURCE_AVAILABLE` (Bit 5) bei Motion
- `LIGHT_VALUE_AVAILABLE` (Bit 6) bei VEML7700-OK
- `LIGHT_GUARD_ENABLED` (Bit 7) immer
- `AUTO_RELAY_OWNED` (Bit 1) bei Auto-Steuerung
- `BLOCKED_BY_LUX` (Bit 3) bei blockiertem Auto-On
- `0x10` (Bit 4, hall-spezifisch) bei Pending-Entscheidung ohne Lux

### `netErlDeviceGetCachedLux(uint16_t* luxOut)`
Liefert den zuletzt gemessenen Luxwert für sofortige Auto-On-Entscheidung (ohne auf den nächsten Sensor-Poll zu warten).

### `netErlDeviceHasSensorFault()`
Gibt `true` zurück, wenn BME280 oder VEML7700 nicht verfügbar sind.

### `netErlDeviceLogSnapshot()`
Schreibt kompaktes Debug-Log: Temperatur, Feuchte, Lux, Motion, Relais, Auto-Flags, Fehlerstatus.

---

## 9. Konfigurationsparameter (via Provisioning/CFG)

| Parameter-ID (hex) | Name | Default | Bereich | Beschreibung |
|---|---|---|---|---|
| `SH_CFG_REPORT_INTERVAL_S` (0x02) | `report_interval_s` | 10 s | 5–600 s | Periodisches STATE-Intervall |
| `SH_CFG_LIGHT_THRESHOLD_ON` (0x22) | `auto_on_lux_threshold` | 250 Lux | 0–65535 | Einschaltschwelle (Lux ≤ Wert → EIN) |
| `SH_CFG_AUTO_OFF_DELAY_S` (0x21) | `auto_off_delay_s` | 15 s | 0–65535 | Nachlaufzeit nach letzter Bewegung |

### Persistenz

Die Konfiguration wird im NVS-Namespace `net_erl_hl` unter dem Key `hall_setup_v1` gespeichert.

```
NetErlPersistedData:
  uint32_t magic        = 0x484C4C31
  uint16_t version      = 1
  uint16_t reserved
  uint16_t autoOnLuxThreshold
  uint16_t autoOffDelayS
```

---

## 10. Design-Entscheidungen

### BME280 ohne Druck-Capability
Der BME280 unterstützt zwar Druckmessung, wurde aber bewusst aus dem Capability-Vertrag ausgeschlossen (`SH_CAP_PRESSURE` nicht gesetzt). Der Flur ist kein geeigneter Ort für aussagekräftige Druckmessung (Türen, Zugluft). Der Gaswiderstand (BME680-Feature) steht ohnehin nicht zur Verfügung.

### Late-Lux-Logik
Die Auto-On-Entscheidung wird nicht verzögert, wenn bereits ein gecachter Luxwert vorliegt (präferiert). Nur wenn der VEML7700 beim ersten Motion-HIGH noch keinen gültigen Wert geliefert hat (z.B. direkt nach dem Boot), bleibt die Entscheidung aus und wird beim nächsten Sensor-Poll getroffen. Dies vermeidet Fehlentscheidungen bei "blinder" Einschaltung.

### Kein Button
Das Hall Module hat keinen lokalen Bedientaster. Die Steuerung erfolgt ausschließlich über Auto-Light oder Master-Kommandos. Der Setup-Button (GPIO2) dient nur der Inbetriebnahme.

### Status-LED nicht bestückt
`PIN_STATUS_LED = -1` — Die Platine hat keine Status-LED vorgesehen. Die Setup-Indikator-LED (GPIO6) ist nur im Setup-Modus aktiv.

### Nachlauf-Verlängerung bei jeder Bewegung
`NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION = 1`: Jede erneute PIR-Erkennung setzt den 15-Sekunden-Nachlauf-Timer zurück. Dies verhindert ungewolltes Ausschalten während andauernder Anwesenheit, z.B. bei langen Flurgängen.

### ISR-sichere CMD-Queue
`NET_ERL_USE_ISR_CMD_QUEUE = 1`: Der ESP-NOW-Recv-Callback läuft im Interrupt-Kontext. Die CMD wird in einen geschützten Puffer kopiert und in `loop()` verarbeitet (`verarbeiteAusstehende()`). Dies verhindert Probleme mit Serial, NVS oder delay() im ISR-Kontext.

### WDT-Timeout
`NET_ERL_WDT_TIMEOUT_S = 8`: Der Task-Watchdog wird alle 20 ms in `loop()` zurückgesetzt. Bei Hängern erfolgt ein Panic-Reset.

---

## 11. Sensor-Masken-Kodierung

Die Sensor-Maske `THLMXXXXXX` kodiert die verfügbaren Sensoren positionsbasiert:

| Position | Zeichen | Sensor | Bedeutung |
|---|---|---|---|
| 0 | `T` | Temperatur | BME280 |
| 1 | `H` | Feuchte | BME280 |
| 2 | `L` | Lux | VEML7700 |
| 3 | `M` | Motion | PIR (GPIO7) |
| 4–9 | `X` | — | Nicht vorhanden |

Die Input-Maske `XXXXX` zeigt an, dass kein lokaler Button vorhanden ist.
