# NET-ERL-002 — Hall Module LED Ring

**Device ID:** `NET-ERL-002`  
**Device Name:** `NET-ERL Hall Module LED Ring`  
**FW-Variante:** `net_erl_hall_module_led_ring`  
**Config-Profil:** `HALL_MODULE_LED_RING` (`SH_PROFILE_HALL_MODULE_LED_RING`)  
**Steuerungsmodus:** `RELAY_LIGHT` (`SH_CONTROL_MODE_RELAY_LIGHT`)  
**Reporting-Modus:** `HYBRID` (periodisch + ereignisgesteuert)  
**Geräteklasse:** `SH_CLASS_NET_ERL` (0x01)  
**Stromversorgung:** `SH_POWER_MAINS` (Netzbetrieben)

---

## 1. Übersicht

Das NET-ERL-002 Hall Module LED Ring ist die leistungsfähigste NET-ERL-Variante und das umfangreichste Gerät im Smarthome-Projekt. Es kombiniert einen LD2410-Radar-Präsenzsensor, BME680 (Temperatur, Feuchte, Druck, Gas), VEML7700 (Lux), ENS160 (AQI, TVOC, eCO2), einen lokalen Taster und einen 17-LED-NeoPixel-Ring. Der LED-Ring visualisiert die Luftqualität (AQI) als Primärfunktion, ergänzt durch Komfortanzeigen für Temperatur und Feuchte.

### Fähigkeiten (Capabilities)

| Capability | Bit | Beschreibung |
|---|---|---|
| `SH_CAP_RELAY` | 0x0001 | Ein Relaisausgang |
| `SH_CAP_TEMP` | 0x0004 | Temperatursensor (BME680) |
| `SH_CAP_HUM` | 0x0008 | Luftfeuchtesensor (BME680) |
| `SH_CAP_LUX` | 0x0010 | Helligkeitssensor (VEML7700) |
| `SH_CAP_MOTION` | 0x0040 | LD2410-Radar-Präsenz |
| `SH_CAP_AQI` | 0x0020 | Luftqualitätssensor (ENS160) |
| `SH_CAP_PRESSURE` | 0x8000 | Luftdrucksensor (BME680) |
| `SH_CAP_BUTTON` | 0x0400 | Lokaler Taster (GPIO6) |
| `SH_CAP_LED_RING` | 0x1000 | WS2812-LED-Ring (17 LEDs) |

---

## 2. Hardware-Pin-Belegung

| Funktion | GPIO | Typ | Beschreibung |
|---|---|---|---|
| Relais 1 | 10 | OUTPUT, active-HIGH | Schaltausgang für Beleuchtung |
| I2C SDA | 0 | I2C | BME680 + VEML7700 + ENS160 Datenleitung |
| I2C SCL | 1 | I2C | BME680 + VEML7700 + ENS160 Taktleitung |
| Button | 6 | INPUT, active-LOW | Lokaler Taster (Short-Press: Relais toggeln, Long-Press: Setup-Modus) |
| LD2410 OUT | 7 | INPUT | RADAR-Präsenz-Ausgang (HIGH = Präsenz) |
| NeoPixel LED-Ring | 4 | OUTPUT (WS2812) | 17 LEDs, GRB, 800 kHz |
| LD2410 UART RX | 20 | UART RX | (reserviert, nicht aktiv genutzt) |
| LD2410 UART TX | 21 | UART TX | (reserviert, nicht aktiv genutzt) |
| Setup/Status-LED | -1 | — | Nicht bestückt |

**Relais-Polarität:** `RELAY_1_ACTIVE_HIGH = 1` — HIGH (3,3 V) schaltet das Relais ein.

---

## 3. Timing-Parameter

| Parameter | Wert | Beschreibung |
|---|---|---|
| HELLO-Retry-Intervall | 5000 ms | Wiederholung der HELLO-Nachricht bei Master-Suche |
| Heartbeat-Intervall | 20000 ms | Periodisches Lebenszeichen |
| Loop-Intervall | 20 ms | Hauptschleifen-Pause |
| Sensor-Poll-Intervall (LD2410) | 50 ms | Abfrageintervall des Radar-Sensors |
| Env-Sample-Intervall | 60000 ms (60 s) | Abstand zwischen Umweltsensor-Messungen |
| Recovery-Retry-Intervall | 30000 ms (30 s) | Wiederholungsintervall für defekte Sensoren |
| Default-Report-Intervall | 10 s | Standard-Zustandsreport-Intervall |
| Min/Max Report-Intervall | 5 s / 600 s | Konfigurierbare Grenzen |
| Auto-Off-Delay | 15 s (default) | Nachlaufzeit nach letzter Präsenz |
| Auto-On-Lux-Schwelle | 250 Lux (default) | Helligkeitsschwelle für automatisches Einschalten |
| I2C-Takt | 5000 Hz (5 kHz) | Reduzierter I2C-Takt für stabilere Kommunikation |
| Button-Debounce | 40 ms | Entprellzeit des lokalen Tasters |

### Gas-/ENS160-Warmup

| Parameter | Wert | Beschreibung |
|---|---|---|
| BME680 Gas-Warmup | 180 s (3 min) | Wartezeit vor Freigabe der Gaswerte |
| BME680 Gas-Min-Reads | 5 | Mindestens 5 gültige Messungen vor Gas-Freigabe |
| BME680 Heizprofil | 320 °C / 150 ms | Heizdauer und -temperatur für eine Gas-Messung |
| ENS160 Warmup | 180 s (3 min) | Wartezeit vor Freigabe der AQI-Werte |
| ENS160 Stale-Timeout | 120 s (2 min) | AQI/TVOC/eCO2 werden ungültig bei ausbleibenden Messwerten |

### LED-Ring-Timing

| Parameter | Wert | Beschreibung |
|---|---|---|
| LED-Ring-Brightness | 24 | Standard-Helligkeit (0–255 skaliert) |
| Max. konfigurierbare Brightness | 96 | Obergrenze der Helligkeit |
| AQI-Phase | 15000 ms (15 s) | Luftqualitätsanzeige (Primärfunktion) |
| Temp-Phase | 15000 ms (15 s) | Temperaturanzeige (Komfort-Erweiterung) |
| Hum-Phase | 15000 ms (15 s) | Feuchteanzeige (Komfort-Erweiterung) |
| Frame-Intervall | 120 ms | Aktualisierungsrate des LED-Rings |
| Lux-Blocked-Alert | 3000 ms (3 s) | Animationsdauer bei Lux-Blockade |

---

## 4. Sensor-Konfiguration

### BME680
- Primäre I2C-Adresse: `0x76`
- Fallback-Adresse: `0x77`
- Temperatur-Oversampling: `BME680_OS_8X`
- Feuchte-Oversampling: `BME680_OS_2X`
- Druck-Oversampling: `BME680_OS_4X`
- IIR-Filter: `BME680_FILTER_SIZE_3`
- Gas-Heater: 320 °C für 150 ms
- Plausibilitätsprüfung Druck: 30.000–110.000 Pa
- Sensor-Masken-Positionen: `T` (Temperatur), `H` (Feuchte), `P` (Druck), `G` (Gas)

### VEML7700
- Gain: `VEML7700_GAIN_1`
- Integrationszeit: `VEML7700_IT_400MS` (400 ms — längere Messung für stabilere Luxwerte)

### ENS160
- Primäre I2C-Adresse: `0x52`
- Fallback-Adresse: `0x53`
- Kompensation: BME680-Temperatur und -Feuchte werden ins Register `0x13` geschrieben
- AQI-Skala: Bevorzugt AQI500 (0–500), Fallback AQI 1–5 (mappiert auf `aqi_raw * 100`)
- Standard-Modus: `ENS160_OPMODE_STD`

### LD2410 Radar (digitaler Modus)
- GPIO7 als digitaler Eingang (OUT-Pin)
- HIGH = Präsenz erkannt
- UART-Schnittstelle (GPIO20/21) reserviert, nicht aktiv für Konfiguration genutzt


### EMA-Filter (Rauschunterdrückung)

Der BME680 liefert auch Temperatur- und Feuchtewerte. Zur Rauschunterdrückung
wird ein exponentiell gleitender Mittelwert (EMA) angewendet:

- **α = 0,2**: 20 % neuer Messwert, 80 % bisheriger Mittelwert
- **Gas-Sensor**: Kein EMA (Gaswiderstand benötigt mehrere Messungen zur Stabilisierung)

### Sensor-Offset

- `NET_ERL_TEMP_OFFSET_01C`: Temperatur-Korrektur in Zehntelgrad
- `NET_ERL_HUM_OFFSET_01PCT`: Feuchte-Korrektur in Zehntelprozent

Der Offset wird NACH dem EMA-Filter angewendet.

---

## 5. Runtime-Konstanten

| Define | Wert | Beschreibung |
|---|---|---|
| `NET_ERL_STORAGE_NS` | `net_erl_hlr` | NVS-Namespace für persistente Daten |
| `NET_ERL_SENSOR_MASK` | `THLPGAMXXX` | Sensor-Maske (T=Temp, H=Hum, L=Lux, P=Pressure, G=Gas, A=AQI, M=Motion) |
| `NET_ERL_INPUT_MASK` | `BXXXX` | Input-Maske (B=Button) |
| `NET_ERL_PERSISTED_MAGIC` | `0x4B544331` | Magic-Number für NVS-Persistenz |
| `NET_ERL_PERSISTED_KEY` | `hall_led_cfg_v1` | NVS-Key für Konfiguration |
| `NET_ERL_HAS_BUTTON` | definiert | Lokaler Taster vorhanden |
| `NET_ERL_HAS_INDICATOR_UPDATE` | definiert | LED-Ring wird aktualisiert |

---

## 6. ESP-NOW-Protokoll und Payload-Mapping

### 6.1 HELLO-Payload (79 Bytes)

| Feld | Bytes | Wert |
|---|---|---|
| `device_id` | 16 | `NET-ERL-002` |
| `device_name` | 32 | `NET-ERL Hall Module LED Ring` |
| `device_class` | 1 | `SH_CLASS_NET_ERL` (0x01) |
| `caps_hi` | 1 | Pressure + LED_Ring + Button |
| `caps_lo` | 1 | Relay + Temp + Hum + Lux + AQI + Motion |
| `power_type` | 1 | `SH_POWER_MAINS` |
| `sensor_mask` | 11 | `THLPGAMXXX` |
| `input_mask` | 6 | `BXXXX` |
| `config_profile` | 1 | `SH_PROFILE_HALL_MODULE_LED_RING` (0x04) |

### 6.2 STATE-Payload: ExtendedRelayComfortGasConfigStateReportPayload (45 Bytes)

Größter Payload im System. Enthält alle Sensorwerte – sowohl `pressure_pa` (Luftdruck) als auch `gas_ohm` (BME680-Gaswiderstand) sind Bestandteil des Payloads.

```
Offset  Größe  Feld                  Typ      Beschreibung
------  -----  --------------------  -------  -----------------------------------------
 0      16     node_id               char[]   Geräte-ID "NET-ERL-002"
16       1     relay_1               uint8_t  Relais-Zustand (0=AUS, 1=EIN)
17       2     temp_01c              int16_t  Temperatur in 1/10 °C
19       2     hum_01pct             uint16_t Relative Feuchte in 0,1 %
21       2     lux                   uint16_t Beleuchtungsstärke in Lux
23       4     pressure_pa           uint32_t Luftdruck in Pascal
27       4     gas_ohm               uint32_t BME680-Gaswiderstand in Ohm
31       2     aqi                   uint16_t Luftqualitätsindex (0–500)
33       2     tvoc_ppb              uint16_t Flüchtige organische Verbindungen (ppb)
35       2     eco2_ppm              uint16_t CO₂-Äquivalent (ppm)
37       1     motion                uint8_t  Präsenz (0=keine, 1=erkannt)
38       1     auto_flags            uint8_t  Bitmaske (siehe 6.3)
39       1     fault                 uint8_t  Fehlerstatus
40       1     _pad                  uint8_t  Padding
41       2     report_interval_s     uint16_t Aktuelles Report-Intervall (Sekunden)
43       2     auto_on_lux_threshold uint16_t Aktuelle Einschaltschwelle (Lux)
```

### 6.3 Auto-Flags Bitmaske

| Bit | Flag | Beschreibung |
|---|---|---|
| 0 | `AUTO_REQUEST_ON` | Auto-On-Entscheidung steht aus |
| 1 | `AUTO_RELAY_OWNED` | Relais von Auto-Light gesteuert |
| 2 | `BLOCKED_BY_SERVER` | (nicht verwendet) |
| 3 | `BLOCKED_BY_LUX` | Auto-On wegen zu hoher Helligkeit blockiert |
| 4 | `BLOCKED_BY_MISSING_LUX` | Pending-Entscheidung, Lux fehlt |
| 5 | `PRESENCE_SOURCE_AVAILABLE` | LD2410 meldet Präsenz |
| 6 | `LIGHT_VALUE_AVAILABLE` | VEML7700 verfügbar |
| 7 | `LIGHT_GUARD_ENABLED` | Luxschutz immer aktiv |

**Unterschied zu NET-ERL-001:** Bit 4 verwendet den definierten `SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX` (0x10) statt des hall-spezifischen Werts.

### 6.4 EVENT-Payload: EventReportPayload (22 Bytes)

Zusätzliche Events gegenüber NET-ERL-001:
- `SH_EVENT_BUTTON_PRESS` / `SH_EVENT_BUTTON_RELEASE` bei Taster-Bedienung (Trigger: `SH_TRIGGER_MANUAL_BUTTON`)

---

## 7. MQTT-Topics (abgeleitet)

```
smarthome/NET-ERL-002/state        → 45 Bytes ExtendedRelayComfortGasConfigStateReportPayload
smarthome/NET-ERL-002/event        → 22 Bytes EventReportPayload
smarthome/NET-ERL-002/cmd          → 4 Bytes CmdPayload
smarthome/NET-ERL-002/cfg          → 4 Bytes CfgPayload
smarthome/NET-ERL-002/hello        → 79 Bytes HelloPayload
smarthome/NET-ERL-002/heartbeat    → 20 Bytes HeartbeatPayload
smarthome/NET-ERL-002/available    → Online/Offline
```

Vom Master aufgelöste Felder:
```
smarthome/NET-ERL-002/temperature      → float (temp_01c / 10)
smarthome/NET-ERL-002/humidity         → float (hum_01pct / 10)
smarthome/NET-ERL-002/lux              → uint16_t
smarthome/NET-ERL-002/pressure         → float (pressure_pa / 100)
smarthome/NET-ERL-002/gas_resistance   → uint32_t (Ohm)
smarthome/NET-ERL-002/aqi              → uint16_t
smarthome/NET-ERL-002/tvoc             → uint16_t (ppb)
smarthome/NET-ERL-002/eco2             → uint16_t (ppm)
smarthome/NET-ERL-002/motion           → boolean
smarthome/NET-ERL-002/relay            → boolean
smarthome/NET-ERL-002/button           → boolean
```

---

## 8. Auto-Light-Zustandsautomat

Der Auto-Light-Mechanismus ist identisch zu NET-ERL-001 (siehe NET-ERL-001 Dokumentation, Kapitel 7). Die Unterschiede:

- **Präsenzquelle:** LD2410-Radar statt PIR — empfindlicher und mit größerer Reichweite
- **Sensor-Poll-Intervall:** 50 ms (deutlich schneller als 250 ms beim PIR)
- **Gecachter Lux:** Wird beim ersten Radar-HIGH sofort verwendet (tryAutoOnFromCachedLux)
- **Late-Lux:** Nach 60 s (Env-Sample-Intervall) wird der nächste VEML7700-Wert abgewartet

---

## 9. LED-Ring-Anzeige

### 9.1 Phasen-Modell

Der LED-Ring durchläuft nach Erkennung einer Präsenz drei Phasen (Gesamtdauer: 45 s):

```
Präsenz erkannt
    │
    ├── AQI-Phase (15 s) ─── Primärfunktion
    │     Visualisiert die Luftqualität via AQI-Farbcode
    │     Farbe pulsiert mit Sinus-Welle
    │
    ├── Temperatur-Phase (15 s) ─── Komfort-Erweiterung
    │     Anzahl leuchtender LEDs proportional zur Temperatur
    │     Laufender Sweep-Balken als Highlight
    │
    └── Feuchte-Phase (15 s) ─── Komfort-Erweiterung
          Anzahl leuchtender LEDs proportional zur Feuchte
          Wellenanimation (crest = heller)
```

Nach 45 s erlischt der Ring. Jede neue Präsenz startet die Sequenz neu (wenn sie bereits abgelaufen ist oder überschrieben wird).

### 9.2 AQI-Farbcode

| AQI-Bereich | Farbe | RGB (skaliert) | Bedeutung |
|---|---|---|---|
| Ungültig | Blau | 0, 70, 220 | Sensor nicht bereit |
| ≤ 50 | Grün | 0, 210, 80 | Gute Luftqualität |
| ≤ 100 | Gelb | 180, 190, 0 | Moderate Luftqualität |
| ≤ 150 | Orange | 255, 115, 0 | Ungesund für empfindliche Gruppen |
| ≤ 200 | Rot | 220, 0, 0 | Ungesund |
| ≤ 300 | Violett | 130, 0, 170 | Sehr ungesund |
| > 300 | Dunkelrot | 90, 0, 40 | Gefährlich |

Die Farbe pulsiert mit einer Sinus-Welle (`pulse = 0.55 + 0.45 * sin(nowMs / 360.0)`) für eine organische, nicht flackernde Anzeige.

### 9.3 Sonderzustände

| Zustand | Anzeige | Dauer | Priorität |
|---|---|---|---|
| Sensor-Fehler | Dauerhaft rot | Solange Fehler besteht | Höchste |
| Lux-Blockade | Orange Lauflicht (4 LEDs) | 3 s | Mittel |
| Keine Präsenz | Aus | — | Niedrigste |

### 9.4 Helligkeitsanpassung

```
ringScale(value) = (value * NET_ERL_LED_RING_BRIGHTNESS) / 255
```

Die Helligkeit ist auf `NET_ERL_LED_RING_BRIGHTNESS = 24` standardisiert und durch `NET_ERL_LED_RING_MAX_CFG_BRIGHTNESS = 96` begrenzt.

### 9.5 Funktionsumfang

Laut Quellcode-Kommentierung:
- **AQI-Phase:** Primärfunktion
- **Temperatur-/Feuchte-Phasen:** Reine Komfort-Erweiterungen, nicht bewertungsrelevant
- **Lux-Blocked-Alert und Sensorfault-Anzeige:** Diagnosehilfen/Komfort, nicht bewertungsrelevant
- Die AQI-Anzeige ändert weder das ESP-NOW-Protokoll noch den Serververtrag

---

## 10. Device-Hook-Implementierungen

### `netErlDeviceInit()`
Initialisiert I2C-Bus mit 5000 Hz (5 kHz) Takt und 50 ms Timeout, BME680 mit Gas-Heater (320 °C/150 ms), VEML7700 mit 400 ms Integrationszeit, ENS160 mit Fallback-Adress-Scan, LD2410 OUT als INPUT, NeoPixel-Ring.

### `netErlDeviceResetSensorDefaults()`
Setzt alle Sensorwerte auf ungültige Markierungen:
- `temp_01c = INT16_MIN`, `hum_01pct = 0xFFFF`, `lux = 0xFFFF`
- `pressure_pa = 0xFFFFFFFF`, `gas_ohm = 0xFFFFFFFF`
- `aqi = 0xFFFF`, `tvoc_ppb = 0xFFFF`, `eco2_ppm = 0xFFFF`

### `netErlDeviceReadPresence()`
Liest LD2410 OUT-Pin (GPIO7). Bei HIGH: startet `startRingComfortSequence()` für den LED-Ring. Ruft danach `updateRingComfortDisplay()` auf.

### `netErlDeviceSetRelayOutput(bool on)`
Setzt Relais-Pin (GPIO10) entsprechend der konfigurierten Polarität.

### `netErlDeviceUpdateIndicators(bool relayOn)`
Ruft `updateRingComfortDisplay()` auf. Der relayOn-Parameter wird ignoriert (die LED-Ring-Anzeige ist unabhängig vom Relais-Zustand).

### `netErlDevicePollSensors(unsigned long nowMs)`
Komplexester Sensor-Poll im gesamten System:
1. **Rate-Limit:** 60 s zwischen Umweltsensor-Polls
2. **Recovery:** Alle 30 s für BME680, VEML7700 und ENS160
3. **BME680:** Temp/Feuchte/Druck/Gas mit Plausibilitätsprüfung (Druck 30.000–110.000 Pa)
   - Gaswarmup: 180 s + mindestens 5 gültige Messungen
   - Gaswiderstand wird nur nach Warmup gemeldet
4. **VEML7700:** Lux mit uint16_t-Clamping
5. **ENS160-Kompensation:** BME680-Temperatur/Feuchte in Register 0x13
6. **ENS160-Messung:** AQI500 bevorzugt, AQI 1–5 Fallback auf 0–500 skaliert
   - Stale-Detection nach 120 s ohne gültige Messung
7. **Late-Lux:** Gleiche Logik wie NET-ERL-001
8. **LED-Ring-Update:** Nach Sensor-Poll
9. **Delta-Detektion:** Temp (jede Änderung), Hum (≥5), Lux (≥5), Pressure (≥10 Pa), AQI (jede Änderung)

### `netErlDeviceFillStatePayload(void* payload, size_t* size)`
Befüllt `ExtendedRelayComfortGasConfigStateReportPayload` (45 Bytes).

### `netErlDeviceBuildAutoFlags()`
Wie NET-ERL-001, aber Bit 4 verwendet `SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX` (0x10).

### `netErlDeviceGetCachedLux(uint16_t* luxOut)`
Gleicht NET-ERL-001.

### `netErlDeviceHasSensorFault()`
Gibt `true` bei Fehler von BME680, VEML7700 oder ENS160 (oder ENS160-Warmup abgeschlossen aber keine gültigen Daten).

### `netErlDeviceLogSnapshot()`
Loggt alle 14 Messwerte: Temperatur, Feuchte, Lux, Druck, Gas-OHM, AQI, TVOC, eCO2, Motion, Relais.

### `netErlDeviceReadButton()`
Liest GPIO6, berücksichtigt `BUTTON_1_ACTIVE_LOW`.

---

## 11. Konfigurationsparameter

| Parameter-ID (hex) | Name | Default | Bereich | Beschreibung |
|---|---|---|---|---|
| `SH_CFG_REPORT_INTERVAL_S` (0x02) | `report_interval_s` | 10 s | 5–600 s | Periodisches STATE-Intervall |
| `SH_CFG_LIGHT_THRESHOLD_ON` (0x22) | `auto_on_lux_threshold` | 250 Lux | 0–65535 | Einschaltschwelle |
| `SH_CFG_AUTO_OFF_DELAY_S` (0x21) | `auto_off_delay_s` | 15 s | 0–65535 | Nachlaufzeit |

### Persistenz

NVS-Namespace `net_erl_hlr`, Key `hall_led_cfg_v1`.

```
NetErlPersistedData:
  uint32_t magic        = 0x4B544331
  uint16_t version      = 1
  uint16_t reserved
  uint16_t autoOnLuxThreshold
  uint16_t autoOffDelayS
```

---

## 12. Design-Entscheidungen

### I2C-Takt 5000 Hz (5 kHz)
`NET_ERL_I2C_CLOCK_HZ = 5000`: Der I2C-Bus wird auf nur 5 kHz reduziert. Grund: Drei Sensoren (BME680, VEML7700, ENS160) teilen sich einen Bus. Längere Leitungen und mögliche Störeinkopplungen werden durch den niedrigen Takt entschärft.

### BME680 Gas-Heater 320 °C / 150 ms
Der Gaswiderstand wird erst nach 180 s Warmup und 5 gültigen Messungen gemeldet. Dies verhindert falsche Gaswerte während der Aufwärmphase des Heizelements.

### ENS160-Fallback-Adressen
Der ENS160 wird an zwei möglichen Adressen gesucht (0x52 primär, 0x53 Fallback). Dies erhöht die Toleranz gegenüber unterschiedlichen Sensor-Bestückungen.

### ENS160-Kompensation
Temperatur und Feuchte vom BME680 werden direkt in das ENS160-Register 0x13 geschrieben. Der ENS160 nutzt diese Daten für eine genauere AQI-Berechnung (Kompensation von Temperatur- und Feuchtedrift des Gassensors).

### VEML7700 mit 400 ms Integrationszeit
Gegenüber 100 ms beim NET-ERL-001 wird hier eine längere Integrationszeit verwendet. Grund: Der LED-Ring kann Streulicht verursachen; eine längere Mitteilung stabilisiert die Lux-Messung.

### LED-Ring als lokale Anzeige
Der NeoPixel-Ring kommuniziert keine Daten über ESP-NOW oder MQTT. Er ist eine rein lokale Visualisierung. Die AQI-Anzeige (Primärfunktion) ist durch die Funktion `ringAqiColor()` und die AQI-Phase klar von den Komfort-Erweiterungen (Temperatur, Feuchte) getrennt.

### Keine Status-LED
`PIN_STATUS_LED = -1`: Der LED-Ring übernimmt die visuelle Rückmeldung. Eine separate Status-LED ist nicht bestückt.

### Button als kombinierter Setup-/Bedientaster
GPIO6 dient sowohl als Setup-Button (5000 ms Long-Press → Setup-Modus) als auch als Bedientaster (Short-Press → Relais toggeln). Die Unterscheidung erfolgt in `processBtn()` über die Haltezeit.

### Motorfolgelogik (Manual-Follow-Motion)
Wie NET-ERL-001: Bei manuellem Einschalten (Server oder Button) wird eine Motion-Folgelogik aktiviert. Das Relais bleibt an, solange Präsenz erkannt wird. Nach 30 Minuten ohne Präsenz startet ein 15-Sekunden-Prüffenster. Bei ausbleibender Präsenz in diesem Fenster wird das Licht ausgeschaltet.

---

## 13. Vergleich: NET-ERL-001 vs. NET-ERL-002

| Merkmal | NET-ERL-001 | NET-ERL-002 |
|---|---|---|
| Motion-Sensor | PIR (GPIO7) | LD2410 Radar (GPIO7) |
| Motion-Poll | 250 ms | 50 ms |
| Temperatur/Feuchte | BME280 | BME680 |
| Luftdruck | — | BME680 (30k–110k Pa) |
| Gaswiderstand | — | BME680 (180 s Warmup) |
| Luftqualität (AQI) | — | ENS160 (AQI500, TVOC, eCO2) |
| Lux | VEML7700 (100 ms IT) | VEML7700 (400 ms IT) |
| Button | — | GPIO6 (Short/Long-Press) |
| LED-Ring | — | 17× NeoPixel WS2812 |
| STATE-Payload | 31 Bytes | 45 Bytes |
| Sensor-Maske | `THLMXXXXXX` | `THLPGAMXXX` |
| Input-Maske | `XXXXX` | `BXXXX` |
| Config-Profil | `HALL_LIGHT` | `HALL_MODULE_LED_RING` |
| Capabilities | 5 | 9 |
| I2C-Takt | 100 kHz (Standard) | 5 kHz |
| CMD-Queue | ISR-safe aktiv | (Standard, nicht ISR-optimiert) |
| OFT-Timer-Extend | Ja | Ja |
