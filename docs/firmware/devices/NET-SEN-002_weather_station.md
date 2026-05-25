# NET-SEN-002 — Wetterstation (BME280 + VEML7700 + Regen)

## Geräteidentifikation

| Feld | Wert |
|---|---|
| **DEVICE_ID** | `NET-SEN-002` |
| **DEVICE_NAME** | `NET-SEN Env BME280+VEML+Rain` |
| **FW_VARIANT** | `net_sen_weather_station` |
| **CAPS** | `TEMP \| HUM \| LUX \| PRESSURE \| RAIN` |
| **REPORTING_MODE** | `HYBRID` (periodisch + eventgesteuert) |
| **Bauform** | Vernetzt (Netzteil), kein Batteriebetrieb |

## Hardware-Pins

| Signal | GPIO | Typ | Anmerkung |
|---|---|---|---|
| I2C SDA | 0 | Bidirektional | BME280 + VEML7700 |
| I2C SCL | 1 | Bidirektional | BME280 + VEML7700 |
| Rain Digital | 3 | Eingang | Active LOW, INPUT_PULLUP |
| SETUP_BUTTON | 2 | Eingang | Active LOW, 5000 ms Haltezeit |
| SETUP_LED | 7 | Ausgang | Active HIGH, 500 ms Blink |
| STATUS_LED | -1 | — | Nicht belegt |
### Setup-Taster

- **5 Sekunden halten**: Aktiviert den Setup-Modus
- **Setup-Modus**: Das Gerät startet einen WiFi-Access-Point.
  SSID = Geräte-ID (z.B. `NET-SEN-002`), Passwort = `smarthome`.
  Über das Webinterface (`192.168.4.1`) können Master-MAC und
  Konfigurationswerte eingestellt werden.
- **Während Setup**: Sensorik und Funk-Kommunikation pausieren.
- **Status-LED**: Blinkt während des Setup-Modus schnell (200 ms Intervall).
- **Timeout**: Nach 5 Minuten ohne Speichern wird der Setup-Modus automatisch
  beendet und das Gerät startet neu.
- **Normale Funktion**: Kurzes Drücken (< 5 s) hat keine Wirkung im Normalbetrieb.

## Konfigurationsparameter

### Timing

| Parameter | Wert | Beschreibung |
|---|---|---|
| `HELLO_RETRY` | 5000 ms | Wiederholung Hello-Nachricht |
| `HEARTBEAT` | 20000 ms | Heartbeat-Intervall |
| `LOOP` | 50 ms | Hauptschleifen-Zyklus |
| `DEFAULT_REPORT_INTERVAL_S` | 10 s | Periodischer Report (HYBRID) |
| `SENSOR_READ_INTERVAL_MS` | 60000 ms | Sensor-Leseabstand (1 min) |
| `ERROR_LOG_INTERVAL_MS` | 15000 ms | Dämpfung Fehlerlog |
| `SNAPSHOT_LOG_INTERVAL_MS` | 30000 ms | Snapshot-Log-Intervall |
| `VEML_FIRST_READ_DELAY` | 1050 ms | Einschwingzeit VEML7700 vor erster Messung |

### Deltas (Sensor-Change-Erkennung)

| Parameter | Rohwert | Physikalisch | Beschreibung |
|---|---|---|---|
| `TEMP_DELTA` | 10 | 1,0 °C | Temperaturänderung löst Event aus |
| `HUM_DELTA` | 50 | 5,0 % rF | Feuchteänderung löst Event aus |
| `LUX_DELTA` | 25 | 25 lx | Helligkeitsänderung löst Event aus |
| `PRESSURE_DELTA` | 30 | 30 Pa | Druckänderung erzeugt Delta im Extended State |

### BME280 Adressen

| Adresse | Verwendung |
|---|---|
| `0x76` | Primäre I2C-Adresse |
| `0x77` | Fallback bei Nichtfinden |
### EMA-Filter (Rauschunterdrückung)

BME280 Temperatur und Feuchte werden über einen EMA-Filter (α=0,2) geglättet.
Luftdruck wird direkt übernommen (träge genug durch physikalische Eigenschaften).

### Sensor-Offset

- `NET_SEN_TEMP_OFFSET_01C`: Temperatur-Korrektur in Zehntelgrad
- `NET_SEN_HUM_OFFSET_01PCT`: Feuchte-Korrektur in Zehntelprozent

## Funktionen (complete API)

### `netSenDeviceSensorInit()`

**Aufrufkontext:** Einmalig beim Start, bevor die Hauptschleife beginnt.

**Ablauf:**
1. I2C-Bus initialisieren
2. `initialisiereBme280()` aufrufen — primäre Adresse `0x76` versuchen, bei Fehlschlag `0x77`
3. `initialisiereVeml7700(jetzt)` aufrufen — `begin()`, dann `konfiguriereVeml7700()`, Warmup-Timestamp setzen
4. Regen-Pin (GPIO3) als `INPUT_PULLUP` konfigurieren
5. Ersten Regen-Pegel einlesen (`digitalRead`)
6. `rainEventPending = true` setzen (initialen Zustand melden)

**Seiteneffekte:** Setzt globale Variablen für BME-Status, VEML-Status und Regen-Rohwert.

---

### `netSenDeviceExtendedStateInit()`

**Aufrufkontext:** Wird direkt nach SensorInit aufgerufen.

**Verhalten:** Derzeit leer — der erweiterte Zustand wird bereits in `SensorInit` vorbereitet.

---

### `netSenDeviceExtendedStatePoll(pp, go, a, t, e)`

**Parameter:**

| Name | Typ | Bedeutung |
|---|---|---|
| `pp` | `int32_t*` | Pressure Pascal (Ausgabe) |
| `go` | `uint16_t*` | Gas Ohms (ungültig — wird nicht gesetzt) |
| `a` | `uint16_t*` | Air Quality Index (ungültig) |
| `t` | `uint16_t*` | TVOC ppb (ungültig) |
| `e` | `uint16_t*` | eCO2 ppm (ungültig) |

**Ablauf:**
- Nur `*pp = pressurePa` setzen (Wert aus letzter Sensor-Messung)
- Alle anderen Parameter bleiben unverändert (Ungültigkeitsmarker)

---

### `netSenDevicePollEvent(et, tr, p1, p2)`

**Parameter (Ausgabe):**

| Name | Typ | Bedeutung |
|---|---|---|
| `et` | `ShEventType` | Event-Typ |
| `tr` | `ShEventTrigger` | Trigger-Ursache |
| `p1` | `int16_t` | Parameter 1 (Status) |
| `p2` | `int16_t` | Parameter 2 (ungenutzt) |

**Ablauf:**
1. Wenn `rainSendPending == true`:
   - `et = SH_EVENT_RAIN_DETECTED`
   - `tr = SH_EVENT_TRIGGER_AUTO`
   - `p1 = (wet ? 1 : 0)` — Regenstatus nass/trocken
   - `p2 = 0`
   - **Rückgabewert: `true`** (Event vorhanden)
2. Sonst: **Rückgabewert: `false`** (kein Event)

---

### `netSenDeviceEventSendResult(sent, et, ...)`

**Aufrufkontext:** Nach dem Sendeversuch eines Events.

**Ablauf:**
- Wenn `et == SH_EVENT_RAIN_DETECTED` und `sent == false`:
  - `rainSendPending = true` (erneut pending setzen — Wiederholung)
- Wenn `sent == true`: Event gilt als bestätigt.

**Begründung:** Rain-Events dürfen nicht verloren gehen. Wird das Senden nicht bestätigt, bleibt das Event pending.

---

### `netSenDeviceSensorPoll(temp_01c, hum_01pct, lux, motion, fault)`

**Parameter (Ausgabe):**

| Name | Typ | Skala |
|---|---|---|
| `temp_01c` | `int16_t*` | 1/10 °C (z. B. 235 = 23,5 °C) |
| `hum_01pct` | `int16_t*` | 1/10 % rF |
| `lux` | `int16_t*` | Lux |
| `motion` | `uint8_t*` | Bewegung (immer 0 — nicht verbaut) |
| `fault` | `uint8_t*` | Fehlerbitmaske |

**Ablauf (detailliert):**

1. **Rate-Limit:** Nur ausführen, wenn seit letztem Aufruf ≥ `SENSOR_READ_INTERVAL_MS` (60 s) vergangen sind.

2. **BME280 Recovery:** Wenn BME im Fehlerzustand und seit letztem Recovery-Versuch ≥ 30 s:
   - `versucheBmeRecovery()` aufrufen

3. **BME280 lesen:**
   - Temperatur, Luftfeuchte, Druck auslesen
   - Plausibilitätsprüfung Druck: 30000–110000 Pa
   - Bei Erfolg: Werte in globale Variablen übernehmen
   - Bei Fehler: `logBmeFehler()` aufrufen (gedämpft)

4. **VEML7700 lesen:**
   - Wenn Warmup (1050 ms ab Initialisierung) abgelaufen:
     - Lux aus `veml7700.readLux()` lesen
     - Bei Erfolg: Wert übernehmen
     - Bei Fehler: `logVemlFehler()` aufrufen (gedämpft), Recovery-Versuch wenn ≥ 30 s
   - Wenn noch in Warmup: Lux = 0, kein Fehler

5. **Regen Digital:**
   - `leseRegenNass()` — aktuelle Regen-Pegel lesen (digital, active LOW)
   - Wenn Pegel sich geändert hat → `rainEventPending = true`

6. **Extended State (Pressure Delta):**
   - `pressureDelta = abs(pressurePa - lastExtStatePressurePa)`
   - Wenn `pressureDelta >= PRESSURE_DELTA` (30 Pa) → `extStateChanged = true`
   - Sonst: `extStatePayChanged = false`

7. **Snapshot Logging:** Alle 30 s einen Snapshot-Log-Eintrag schreiben

8. **Fault-Bestimmung:**
   - `fault = (bmeFailed ? 1 : 0) | (vemlFailed && !vemlInWarmup ? 2 : 0)`

9. **Rückgabewert (`bool`):** `true` wenn eine der Bedingungen zutrifft:
   - Temp-Delta ≥ 10 (1,0 °C)
   - Hum-Delta ≥ 50 (5,0 %)
   - Lux-Delta ≥ 25
   - Motion geändert (hier nie)
   - Fault geändert

---

### Hilfsfunktionen

#### `logBmeFehler()`

Dämpft BME280-Fehlermeldungen auf maximal einen Eintrag pro `ERROR_LOG_INTERVAL_MS` (15 s).

#### `logVemlFehler()`

Dämpft VEML7700-Fehlermeldungen auf maximal einen Eintrag pro `ERROR_LOG_INTERVAL_MS` (15 s).

#### `initialisiereBme280()`

1. Versuche `bme.begin(0x76)`
2. Bei Erfolg: Rückgabe `true`
3. Bei Fehler: Versuche `bme.begin(0x77)`
4. Rückgabe des Ergebnisses

#### `konfiguriereVeml7700()`

Setzt VEML7700 auf:
- `GAIN_1` (geringste Verstärkung für hohe Helligkeiten)
- `IT_100MS` (Integrationszeit 100 ms)

#### `initialisiereVeml7700(jetzt)`

1. `veml7700.begin()` aufrufen
2. `konfiguriereVeml7700()` aufrufen
3. `vemlFirstReadAt = jetzt + VEML_FIRST_READ_DELAY (1050 ms)` setzen
4. `vemlInWarmup = true`

#### `versucheBmeRecovery()`

- Alle 30 s wiederholt
- Ruft `initialisiereBme280()` erneut auf
- Bei Erfolg: Fehlerstatus zurücksetzen

#### `versucheVemlRecovery()`

- Alle 30 s wiederholt
- Ruft `initialisiereVeml7700(jetzt)` erneut auf
- Bei Erfolg: Fehlerstatus zurücksetzen

#### `leseRegenNass()`

- `return !digitalRead(rainPin)` — da active LOW, invertiert der Aufruf

---

## ESP-NOW Payload Mapping

### SensorStateReportPayload (24 Byte)

Verwendet bei Standard-Reports ohne Druck-Change.

| Offset | Größe | Feld | Quelle |
|---|---|---|---|
| 0 | 4 | `node_id` | DeviceConfig |
| 4 | 2 | `temperature_01c` | `temp_01c` aus SensorPoll |
| 6 | 2 | `humidity_01pct` | `hum_01pct` aus SensorPoll |
| 8 | 2 | `lux` | `lux` aus SensorPoll |
| 10 | 2 | `pressure_pa` | `pressurePa` aus letzter Messung |
| 12 | 1 | `rain_detected` | `regenNass` (0/1) |
| 13 | 4 | reserviert | — |
| 17 | 1 | reserviert | — |
| 18 | 2 | reserviert | — |
| 20 | 2 | reserviert | — |
| 22 | 1 | `error_flags` | Fault-Bitmaske |
| 23 | 1 | reserviert | — |

### ExtendedSensorStateReportPayload (34 Byte)

Verwendet wenn sich der Druck (Extended State) geändert hat.

| Offset | Größe | Feld | Quelle |
|---|---|---|---|
| 0–24 | — | Basis wie oben | SensorStateReportPayload |
| 24 | 2 | reserviert | — |
| 26 | 2 | reserviert | — |
| 28 | 4 | `pressure_pa_extended` | ExtState Poll |
| 32 | 2 | reserviert | — |

**Hinweis:** Gas, AQI, TVOC und eCO2 werden nicht unterstützt — die entsprechenden Felder bleiben auf dem Ungültigkeitswert.

---

## Design-Entscheidungen

### Bewusste Rate-Limits

| Limit | Wert | Grund |
|---|---|---|
| Sensor-Leseabstand | 60 s | Wetterdaten ändern sich langsam, spart I2C-Buslast und Strom |
| Fehlerlog-Dämpfung | 15 s | Verhindert Log-Spam bei dauerhaften Sensorfehlern |
| Snapshot-Log | 30 s | Ausreichend für Debugging, nicht zu aufdringlich |
| BME/VEML Recovery | 30 s | Sensor braucht Zeit zur Rückkehr aus Fehlerzustand |

### VEML7700 Warmup von 1050 ms

Der VEML7700 benötigt nach dem Einschalten eine Einschwingzeit, bevor gültige Lux-Werte gelesen werden können. Der erste Leseversuch erfolgt frühestens 1050 ms nach `begin()`. Bis dahin wird Lux = 0 gemeldet und kein VEML-Fehler gesetzt.

### Rain-Event-Wiederholung

Rain-Detection-Events werden bei fehlgeschlagenem Senden automatisch erneut pending gesetzt (`netSenDeviceEventSendResult`). Dadurch wird sichergestellt, dass ein Regenereignis nicht verloren geht, auch wenn die Funkstrecke temporär gestört ist.

### BME280 Fallback-Adressen

Der BME280 ist auf zwei möglichen I2C-Adressen verfügbar (0x76 und 0x77), abhängig vom SDO-Pin. Die Firmware probiert 0x76 zuerst, dann 0x77. Dies erhöht die Kompatibilität mit verschiedenen Breakout-Boards.

### Plausibilitätsprüfung Luftdruck

Der gemessene Luftdruck wird gegen das Intervall 30000–110000 Pa geprüft. Werte außerhalb dieses Bereichs gelten als ungültig und werden verworfen. Dies verhindert die Verarbeitung offensichtlich fehlerhafter Messwerte (z. B. bei Sensorausfall oder Kurzschluss).

### Pressure Delta (30 Pa) für Extended State

Nur der Luftdruck verwendet das Extended-State-Mechanismus. Eine Änderung von ≥ 30 Pa löst ein Update des Extended Payloads aus. Dies ist ausreichend, um wetterbedingte Druckschwankungen abzubilden, ohne bei Rauschen ständig 34-Byte-Pakete zu senden.

### HYBRID Reporting Mode

Im HYBRID-Modus sendet das Gerät:
- **Periodisch** alle `DEFAULT_REPORT_INTERVAL_S` (10 s) einen vollständigen Report
- **Eventgesteuert** bei Erkennung eines Regenereignisses oder einer relevanten Messwertänderung (Temperatur ≥ 1 °C, Feuchte ≥ 5 %, Lux ≥ 25)

Dies kombiniert die Vorteile von regelmäßigen Status-Updates und sofortiger Ereignismeldung.
## MQTT-Topics (Firmware-Linie)

Der Master publiziert für dieses Gerät folgende Topics:

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-SEN-002/meta` | ✅ | Metadaten |
| `smarthome/device/NET-SEN-002/availability` | ✅ | Online-Status |
| `smarthome/device/NET-SEN-002/state` | ✅ | Sensordaten (Temp, Feuchte, Druck, Lux, Regen) |
| `smarthome/device/NET-SEN-002/event` | ❌ | Ereignisse (Regen erkannt) |
| `smarthome/device/NET-SEN-002/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-SEN-002/command` | — | Kommandos (get_state, set_config) |

## Zustandsautomaten

### Rain-Event-State

```
        +-----------+
        |  Idle     |
        +-----+-----+
              |
     Pegeländerung erkannt
              |
              v
   +---------------------+
   | rainEventPending    |
   | rainSendPending     |
   +---------------------+
              |
   PollEvent() aufgerufen
              |
              v
   +---------------------+
   | Event gesendet      |
   +---------------------+
              |
     sendResult(false)?
     → rainSendPending = true
              |
              v
   +---------------------+
   | Wiederholung        |──→ zurück zu PollEvent()
   +---------------------+
```

### Sensorfehler-Recovery

```
BME/VEML fehlgeschlagen
       |
       v
+------------------+
| bmeVemlFailed    |──→ alle 30 s: versuche Recovery
+------------------+        |
       |                    v
       |            +------------------+
       |            | init erneut      |
       |            +------------------+
       |               |         |
       |            Erfolg    Fehler
       |               |         |
       |               v         v
       |        +----------+  +-----------+
       |        | OK,      |  | bleibt in |
       |        | Flags=0  |  | Failed    |
       |        +----------+  +-----------+
       v
Normalbetrieb
```
