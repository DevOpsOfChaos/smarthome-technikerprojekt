# BAT-SEN-02 — Regensensor (Tiefschlaf, 2× AA)

## Geräteidentifikation

| Feld | Wert |
|---|---|
| **DEVICE_ID** | `bat_sen_02` |
| **DEVICE_NAME** | `BAT-SEN Rain` |
| **FW_VARIANT** | `bat_sen_rain_sensor` |
| **CAPS** | `BATTERY \| RAIN` |
| **REPORTING_MODE** | `SLEEP_EVENT` |
| **BATTERY_PROFILE** | `BAT_PROFILE_2X_AA` (2000–3200 mV) |
| **DEFAULT_WAKE_INTERVAL_S** | 900 (15 min) |
| **DEFAULT_RX_WINDOW_MS** | 5000 |
| **GPIO_WAKE** | Deaktiviert (nur Timer-Wake) |
| **SAMPLE_INTERVAL_MS** | 200 |
| **LEVEL_HIGH_IS_WET** | 1 |

## Hardware-Pins

| Signal | GPIO | Typ | Anmerkung |
|---|---|---|---|
| Regensensor (ADC) | 3 | Analog Input | 0–4095 (12 Bit) |
| SETUP_BUTTON | 2 | Eingang | Active LOW, 5000 ms Haltezeit |
| SETUP_LED | 7 | Ausgang | Active HIGH, 500 ms Blink |
| STATUS_LED | -1 | — | Nicht belegt |
| BATTERY_ADC | Standard-Pin | Analog | Spannungsmessung |
### Setup-Taster

- **5 Sekunden halten**: Aktiviert den Setup-Modus
- **Setup-Modus**: Das Gerät startet einen WiFi-Access-Point.
  SSID = Geräte-ID (z.B. `bat_sen_02`), Passwort = `smarthome`.
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
| `DEFAULT_WAKE_INTERVAL_S` | 900 s (15 min) | Timer-basiertes Aufwachen für Batteriereport |
| `DEFAULT_RX_WINDOW_MS` | 5000 ms | Empfangsfenster nach Aufwachen |
| `SAMPLE_INTERVAL_MS` | 200 ms | ADC-Abtastrate im wachen Zustand |

### ADC-Schwellwerte

| Parameter | Rohwert | Beschreibung |
|---|---|---|
| `STATE_DELTA_RAW` | 25 ADC Steps | Mindeständerung im ADC-Rohwert für Event |
| `WET_THRESHOLD_RAW` | 2200 | Nasseschwelle (wenn ≥, dann nass) |
| `CLEAR_THRESHOLD_RAW` | 2050 | Trockenschwelle (wenn ≥, dann bleibt nass) |

### Hysterese-Bedingungen

Bei `LEVEL_HIGH_IS_WET = 1`:
- Bisher trocken (`regenErkannt = 0`): nass bei `raw >= WET_THRESHOLD_RAW`
- Bisher nass (`regenErkannt = 1`): trocken bei `raw < CLEAR_THRESHOLD_RAW`

**Statische Assertions:**
- `STATE_DELTA_RAW > 0`
- Bei `LEVEL_HIGH_IS_WET = 1`: `CLEAR_THRESHOLD_RAW ≤ WET_THRESHOLD_RAW`
- Sonst: `CLEAR_THRESHOLD_RAW ≥ WET_THRESHOLD_RAW`
- GPIO3 muss ein gültiger ADC-Pin sein

### Batterieprofil: 2× AA

| Parameter | Wert |
|---|---|
| Batterietyp | 2× AA (Alkaline) in Serie |
| Spannungsbereich | 2000–3200 mV |
| Entladeschluss | ca. 2 × 1,0 V = 2000 mV |
| Voll | ca. 2 × 1,6 V = 3200 mV |

### GPIO Wake

| Parameter | Wert |
|---|---|
| Wake-Quelle | GPIO3 (ADC-Eingang) |
| GPIO_WAKE | **Deaktiviert** |
| Wake-Verfahren | Nur Timer-basiert (RTC-Timer) |

**Begründung:** Der Regensensor ist ein analoger Sensor. GPIO-Wake arbeitet auf digitalen Flanken und wäre für die Analogwert-Erkennung nicht geeignet. Daher wird nur der Timer-basierte Wake verwendet. Bei jedem Aufwachen (alle 15 min) wird der ADC mehrfach mit 200 ms Abstand abgetastet, um eine reliable Aussage zu treffen.

---

## Funktionen (complete API)

### `device_init_io()`

**Aufrufkontext:** Einmalig beim Start (nach Wake oder Power-On).

**Ablauf:**
1. `pinMode(sensorPin, INPUT)` — ADC-Pin als Eingang konfigurieren
2. `int raw = leseRainRaw()` — ersten ADC-Wert lesen (0–4095)
3. `regenErkannt = istRegenZustand(raw, false)` — initialen Zustand bestimmen (Hysterese startet von "trocken")
4. `letzterRaw = raw`
5. `eventPending = false`

**Seiteneffekte:** Setzt globale Variablen `regenErkannt`, `letzterRaw`, `eventPending`.

---

### `device_poll_inputs()`

**Aufrufkontext:** Wiederholt in der Hauptschleife nach Wake.

**Ablauf:**
1. **Rate-Limit:** Nur ausführen, wenn seit letztem Aufruf ≥ `SAMPLE_INTERVAL_MS` (200 ms) vergangen sind
2. `raw = leseRainRaw()` — ADC lesen (0–4095)
3. `statusGeändert = (istRegenZustand(raw, regenErkannt) != regenErkannt)`
4. `delta = abs(raw - letzterRaw)`
5. Wenn `statusGeändert` **oder** `delta >= STATE_DELTA_RAW`:
   - `eventPending = true`
   - `regenErkannt = istRegenZustand(raw, regenErkannt)`
   - `letzterRaw = raw`

**Erklärung:** Der Regensensor feuert Events entweder bei Schwellwertüberschreitung (Nass/Trocken-Wechsel mit Hysterese) oder bei einer signifikanten Änderung des Rohwerts (≥ 25 ADC Steps). Letzteres erlaubt die Detektion von zunehmender/abnehmender Nässe, auch wenn die Schwelle noch nicht erreicht ist.

---

### `device_build_state_channels(channelBool1, channelU16_1, channelMask1, fault)`

**Parameter (Ausgabe):**

| Name | Typ | Wert |
|---|---|---|
| `channelBool1` | `uint8_t*` | Regenstatus: 1 = nass, 0 = trocken |
| `channelU16_1` | `uint16_t*` | ADC-Rohwert (0–4095) |
| `channelMask1` | `uint16_t*` | Bitmaske (nicht verwendet) |
| `fault` | `uint8_t*` | Fehlerflags (nicht verwendet) |

**Ablauf:**
- `*channelBool1 = regenErkannt`
- `*channelU16_1 = (uint16_t)letzterRaw`

---

### `device_map_event(eventType, trigger, param1, param2)`

**Parameter (Ausgabe):**

| Name | Typ | Bedeutung |
|---|---|---|
| `eventType` | `ShEventType*` | Wird gesetzt auf `SH_EVENT_RAIN_DETECTED` |
| `trigger` | `ShEventTrigger*` | Wird gesetzt auf `SH_EVENT_TRIGGER_AUTO` |
| `param1` | `uint8_t*` | Regenstatus: 1 = nass, 0 = trocken |
| `param2` | `uint8_t*` | ADC-Rohwert (niederwertiges Byte) |

**Ablauf:**
1. Wenn `eventPending == true`:
   - `*eventType = SH_EVENT_RAIN_DETECTED`
   - `*trigger = SH_EVENT_TRIGGER_AUTO`
   - `*param1 = regenErkannt`
   - `*param2 = (letzterRaw & 0xFF)` — niederwertiges Byte des ADC
   - `eventPending = false`
   - Rückgabe: `true`
2. Sonst: Rückgabe `false`

---

### `device_wake_candidates()`

**Rückgabe:** `0`

**Beschreibung:** GPIO-Wake ist deaktiviert. Der Regensensor verwendet ausschließlich den Timer-Wake für das periodische Aufwachen. Daher werden keine GPIO-Wake-Kandidaten gemeldet.

---

### `istRegenZustand(raw, bisherRegen)`

**Parameter:**

| Name | Typ | Bedeutung |
|---|---|---|
| `raw` | `uint16_t` | Aktueller ADC-Rohwert (0–4095) |
| `bisherRegen` | `bool` | Vorheriger Zustand (nass/trocken) |

**Rückgabe:** `bool` — neuer Regenzustand (`true` = nass)

**Logik bei `LEVEL_HIGH_IS_WET = 1`:**

| Bisher | Bedingung | Ergebnis |
|---|---|---|
| Trocken (`false`) | `raw >= WET_THRESHOLD_RAW` | nass (`true`) |
| Trocken (`false`) | `raw < WET_THRESHOLD_RAW` | trocken (`false`) |
| Nass (`true`) | `raw >= CLEAR_THRESHOLD_RAW` | nass (`true`) |
| Nass (`true`) | `raw < CLEAR_THRESHOLD_RAW` | trocken (`false`) |

**Logik bei `LEVEL_HIGH_IS_WET = 0` (invertiert):**

| Bisher | Bedingung | Ergebnis |
|---|---|---|
| Trocken (`false`) | `raw <= WET_THRESHOLD_RAW` | nass (`true`) |
| Trocken (`false`) | `raw > WET_THRESHOLD_RAW` | trocken (`false`) |
| Nass (`true`) | `raw <= CLEAR_THRESHOLD_RAW` | nass (`true`) |
| Nass (`true`) | `raw > CLEAR_THRESHOLD_RAW` | trocken (`false`) |

---

### `leseRainRaw()`

**Ablauf:**
1. `int raw = analogRead(sensorPin)` — ADC lesen
2. `if (raw < 0) raw = 0` — negative Werte abfangen (Sicherheit)
3. `if (raw > 4095) raw = 4095` — auf 12 Bit begrenzen
4. Rückgabe `raw`

---

## ESP-NOW Payload Mapping

### BatteryStateReportPayload (24 Byte)

| Offset | Größe | Feld | Quelle |
|---|---|---|---|
| 0 | 4 | `node_id` | DeviceConfig |
| 4 | 1 | `battery_pct` | Batterieladung in % (0–100) |
| 5 | 2 | `battery_mv` | Batteriespannung in mV |
| 7 | 1 | `window_open` | Immer 0 (kein Fensterkontakt) |
| 8 | 1 | `rain_raw` | ADC-Rohwert (0–255, LSB von `letzterRaw`) |
| 9 | 1 | `button_flags` | Immer 0 |
| 10 | 1 | `fault` | Fehlerflags (0 = kein Fehler) |
| 11–23 | 13 | reserviert | Auf Null |

**Hinweis zum `rain_raw`-Feld:** Der ADC-Rohwert wird im Payload auf 8 Bit begrenzt (nur niederwertiges Byte). Der vollständige 12-Bit-Wert steht im `param2` des Events zur Verfügung.

---

## Design-Entscheidungen

### Nur Timer-Wake (kein GPIO-Wake)

Anders als der Fensterkontakt (BAT-SEN-01) verwendet BAT-SEN-02 ausschließlich den Timer-basierten Wake. Der Grund ist die analoge Natur des Regensensors: GPIO-Wake erkennt nur binäre Flanken (LOW→HIGH oder HIGH→LOW). Da der Regensensor jedoch einen kontinuierlichen Analogwert liefert, wäre ein digitaler Wake nicht möglich, ohne eine externe Triggerschaltung (Komparator) vorzusehen.

**Konsequenz:** Das Gerät wacht nur alle 15 Minuten auf. Bei jedem Wake-Zyklus werden mehrere ADC-Messungen mit 200 ms Abstand durchgeführt, um den aktuellen Nässegrad zuverlässig zu erfassen.

### ADC-Abtastrate 200 ms

`SAMPLE_INTERVAL_MS = 200` — die ADC-Messung wird maximal alle 200 ms wiederholt. Dies verhindert Rauschen und gibt dem ADC-Sample-and-Hold genügend Zeit für stabile Werte. Ein kürzeres Intervall wäre bei analogen Regensensoren nicht sinnvoll, da der Sensorwert eine gewisse Trägheit aufweist.

### Hysterese (WET_THRESHOLD vs. CLEAR_THRESHOLD)

Die Hysterese verhindert Flankenrauschen um den Schwellwert. Der Sensor wird erst als "nass" gemeldet, wenn der ADC-Wert die obere Schwelle (2200) überschreitet. Um wieder als "trocken" zu gelten, muss der Wert unter die niedrigere Schwelle (2050) fallen.

**Beispiel:**
- ADC 2000 → trocken
- ADC 2150 → trocken (Schwelle 2200 noch nicht erreicht)
- ADC 2250 → **nass** (Einschaltschwelle überschritten)
- ADC 2100 → nass (Hysterese: 2050 noch nicht unterschritten)
- ADC 2000 → trocken (Ausschaltschwelle unterschritten)

Dieser Abstand von 150 ADC-Stufen verhindert ein ständiges Umschalten bei leicht schwankenden Werten (z. B. durch Wassertropfen, die langsam trocknen).

### STATE_DELTA_RAW = 25

Neben dem Schwellwertwechsel (nass↔trocken) wird auch eine Änderung des ADC-Rohwerts um ≥ 25 Steps als Event gemeldet. Dies erlaubt die Erkennung von:
- Zunehmender Nässe (prima regnet stärker)
- Abnehmender Nässe (es trocknet)
- Frühwarnung, bevor die Nass-Schwelle erreicht ist

### LEVEL_HIGH_IS_WET = 1

Die Konfiguration `LEVEL_HIGH_IS_WET = 1` bedeutet: Höhere ADC-Werte entsprechen nasserem Zustand. Dies ist typisch für kapazitive oder resistive Regensensoren, deren Widerstand bei Nässe sinkt, sodass die Spannung steigt. Durch die Konfigurierbarkeit können auch Sensoren mit invertierter Charakteristik angeschlossen werden.

### 2× AA Batterieprofil

Zwei AA-Alkaline-Zellen in Serie liefern 2000–3200 mV (nominal 3,0 V bei 2 × 1,5 V). Der ESP32 arbeitet zuverlässig in diesem Bereich. Der Vorteil gegenüber CR2032 ist die höhere Kapazität (AA ≥ 2000 mAh), was längere Batterielaufzeiten ermöglicht. Der Eingangsspannungsbereich des ESP32 (3,0–3,6 V) wird eingehalten, da neue AA-Zellen ca. 1,6 V liefern (3,2 V gesamt).

### Statische Assertions

Die Kompilierzeit-Prüfungen stellen sicher, dass:
1. Der ADC-Pin gültig ist (GPIO3 ist ein ADC2-Kanal beim ESP32)
2. Das Delta ≥ 1 ist (sinnvolle Konfiguration)
3. Die Hysterese-Schwellwerte korrekt zueinander stehen (`CLEAR ≤ WET` bei High=Wet)

Verstöße führen zu einem Compile-Fehler, bevor fehlerhafte Firmware ausgeliefert werden kann.

---

## Hysterese-Zustandsautomat

```
         raw < CLEAR_THRESHOLD
    +──────────────────────────────┐
    │                              │
    v                              │
+-----------+          +-----------+
|  TROCKEN  |          |   NASS    |
| regen=0   |─────────>| regen=1   |
+-----------+  raw >=  +-----------+
               WET_THRESHOLD
    ^                              │
    │                              │
    └──────────────────────────────┘
         raw < CLEAR_THRESHOLD
```

---

## Sleep- / Wake-Verhalten

```
        +-------------------+
        | Deep Sleep        |
        | (CPU aus,        |
        |  RTC-Timer aktiv) |
        +--------+----------+
                 |
          Timer Wake (900 s)
                 |
                 v
        +-------------------+
        | Boot aus RTC      |
        +-------------------+
                 |
                 v
        +-------------------+
        | device_init_io()  |
        | → 1× ADC lesen    |
        | → Hysterese anw.  |
        +-------------------+
                 |
                 v
   +---------------------------+
   | Poll-Schleife (200 ms)    |
   | → ADC lesen, vergleichen |
   | → build_state_channels   |
   | → map_event wenn nötig   |
   | (für ca. 5000 ms)        |
   +---------------------------+
                 |
                 v
        +-------------------+
        | ESP-NOW Send      |
        | (Report + Event)  |
        +-------------------+
                 |
          RX_WINDOW (5 s)
          warten auf ACK?
                 |
                 v
        +-------------------+
        | Deep Sleep        |
        +-------------------+
```

**Hinweis:** Da GPIO-Wake deaktiviert ist, wird der Sensor zwischen zwei Timer-Wake-Zyklen nicht auf Pegeländerungen reagieren. Ein Regenereignis wird erst beim nächsten Aufwachen (maximal 15 Minuten später) erkannt und gemeldet.

---

## MQTT-Topics (Firmware-Linie)

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/bat_sen_02/meta` | ✅ | Metadaten |
| `smarthome/device/bat_sen_02/availability` | ✅ | Online-Status (online/asleep/offline) |
| `smarthome/device/bat_sen_02/state` | ✅ | Batterie + Regenzustand |
| `smarthome/device/bat_sen_02/event` | ❌ | Ereignisse (Regen erkannt) |
| `smarthome/device/bat_sen_02/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/bat_sen_02/command` | — | Kommandos (get_state, set_config) |