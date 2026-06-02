# BAT-SEN-01 — Fensterkontakt (Tiefschlaf, 2x AAA)

## Geräteidentifikation

| Feld | Wert |
|---|---|
| **DEVICE_ID** | `bat_sen_01` |
| **DEVICE_NAME** | `BAT-SEN Window` |
| **FW_VARIANT** | `bat_sen_window_contact` |
| **CAPS** | `BATTERY \| WINDOW` |
| **REPORTING_MODE** | `SLEEP_EVENT` |
| **BATTERY_PROFILE** | `BAT_PROFILE_2X_AAA` (2000–3200 mV) |
| **DEFAULT_WAKE_INTERVAL_S** | 43200 (12 h) |
| **DEFAULT_RX_WINDOW_MS** | 5000 |
| **GPIO_WAKE** | Aktiviert (`WAKE_LEVEL_HIGH = OPEN_LEVEL_HIGH`) |
| **DEBOUNCE_MS** | 35 |
| **OPEN_LEVEL_HIGH** | 1 |

## Hardware-Pins

| Signal | GPIO | Typ | Anmerkung |
|---|---|---|---|
| Fensterkontakt | 3 | Eingang mit Pullup | Wake-fähig, OPEN_LEVEL_HIGH = Kontakt offen |
| SETUP_BUTTON | 2 | Eingang | Active LOW, 5000 ms Haltezeit |
| SETUP_LED | 7 | Ausgang | Active HIGH, 500 ms Blink |
| STATUS_LED | -1 | — | Nicht belegt |
| BATTERY_ADC | Standard-Pin | Analog | Spannungsmessung |
### Setup-Taster

- **5 Sekunden halten**: Aktiviert den Setup-Modus
- **Setup-Modus**: Das Gerät startet einen WiFi-Access-Point.
  SSID = Geräte-ID (z.B. `bat_sen_01`), Passwort = `smarthome`.
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
| `DEFAULT_WAKE_INTERVAL_S` | 43200 s (12 h) | Timer-basiertes Aufwachen für Batterie-/Alive-Report |
| `DEFAULT_RX_WINDOW_MS` | 5000 ms | Empfangsfenster nach Aufwachen |
| `DEBOUNCE_MS` | 35 ms | Entprellzeit für Fensterkontakt |

### Batterieprofil: 2x AAA

| Parameter | Wert |
|---|---|
| Batterietyp | 2x AAA in Reihe |
| Spannungsbereich | 2200–3000 mV |
| Spannungsbereich (roh) | entspricht ADC-Werten lt. Hardware |


### GPIO Wake

| Parameter | Wert |
|---|---|
| Wake-Quelle | GPIO3 (Fensterkontakt) |
| Wake-Level | HIGH (entspricht `OPEN_LEVEL_HIGH`) |
| Zweck | Aufwachen bei Fensteröffnung aus Tiefschlaf |

## Funktionen (complete API)

### `device_init_io()`

**Aufrufkontext:** Wird direkt nach Systemstart aufgerufen.

**Ablauf:**
1. `pinMode(contactPin, INPUT_PULLUP)` — Fensterkontakt-Pin mit internem Pullup konfigurieren
2. `int level = digitalRead(contactPin)` — initialen Pegel einlesen
3. `kontaktOffen = (level == OPEN_LEVEL_HIGH ? 1 : 0)` — initialen Zustand setzen
4. `eventPending = false`

**Seiteneffekte:** Setzt globale Variablen `kontaktOffen` und `eventPending`.

---

### `device_poll_inputs()`

**Aufrufkontext:** Wird nach `SLEEP_EVENT`-Aufwachen oder in der Hauptschleife aufgerufen.

**Ablauf (Entprelllogik):**

```
Raw-Change erkannt?
   ├── Ja → debounceStart = now, warte auf Stabilisierung
   ├── Nein → debounce läuft?
       ├── Ja → Zeit ≥ DEBOUNCE_MS?
       │   ├── Ja → Pegel wirklich geändert?
       │   │   ├── Ja → kontaktOffen = neuerPegel, eventPending = true
       │   │   └── Nein → nichts tun
       │   └── Nein → abwarten
       └── Nein → nichts tun
```

**Im Detail:**
1. `raw = digitalRead(contactPin)` — aktuellen Pin-Pegel lesen
2. Wenn `raw != lastRawLevel`:
   - `debounceStart = millis()` — Entprell-Timer starten
   - `lastRawLevel = raw`
3. Wenn `debounceStart != 0` (Entprellung aktiv):
   - Wenn `millis() - debounceStart >= DEBOUNCE_MS` (35 ms erreicht):
     - `neuerStatus = (raw == OPEN_LEVEL_HIGH ? 1 : 0)`
     - Wenn `neuerStatus != kontaktOffen`:
       - `kontaktOffen = neuerStatus`
       - `eventPending = true`
     - `debounceStart = 0` (Timer zurücksetzen)

---

### `device_build_state_channels(channelBool1, channelU16_1, channelMask1, fault)`

**Parameter (Ausgabe):**

| Name | Typ | Wert |
|---|---|---|
| `channelBool1` | `uint8_t*` | Fensterkontakt-Status: 1 = offen, 0 = geschlossen |
| `channelU16_1` | `uint16_t*` | nicht gesetzt (0) |
| `channelMask1` | `uint16_t*` | Bitmaske (nicht verwendet) |
| `fault` | `uint8_t*` | Fehlerflags (nicht verwendet) |

**Ablauf:**
- `*channelBool1 = kontaktOffen`

---

### `device_map_event(eventType, trigger, param1, param2)`

**Parameter (Ausgabe):**

| Name | Typ | Bedeutung |
|---|---|---|
| `eventType` | `ShEventType*` | Wird gesetzt auf... |
| `trigger` | `ShEventTrigger*` | Wird gesetzt auf... |
| `param1` | `uint8_t*` | Fensterstatus (1 = offen) |
| `param2` | `uint8_t*` | nicht verwendet |

**Ablauf:**
1. Wenn `eventPending == true`:
   - `*eventType = (kontaktOffen ? SH_EVENT_WINDOW_OPENED : SH_EVENT_WINDOW_CLOSED)`
   - `*trigger = SH_TRIGGER_AUTO`
   - `*param1 = kontaktOffen`
   - `*param2 = 0`
   - `eventPending = false`
   - Rückgabe: `true`
2. Sonst: Rückgabe `false`

---

### `device_wake_candidates()`

**Rückgabe:** `1 << GPIO3` (Bitmaske mit GPIO3 als Wake-Quelle)

**Beschreibung:** Gibt die Wake-Quellen an. GPIO3 (Fensterkontakt) kann das Gerät aus dem Tiefschlaf wecken, wenn der Pegel von LOW auf HIGH wechselt (Fenster öffnet).

---

## ESP-NOW Payload Mapping

### BatteryStateReportPayload (24 Byte)

| Offset | Größe | Feld | Quelle |
|---|---|---|---|
| 0 | 16 Byte (SH_DEVICE_ID_LEN) | `node_id` | DeviceConfig |
| 4 | 1 | `battery_pct` | Batterieladung in % (0–100) |
| 5 | 2 | `battery_mv` | Batteriespannung in mV |
| 7 | 1 | `window_open` | `kontaktOffen` — 1 = offen, 0 = zu |
| 8 | 1 | `rain_raw` | Immer 0 (kein Regensensor) |
| 9 | 1 | `button_flags` | Immer 0 (kein Button-Event im Payload) |
| 10 | 1 | `fault` | Fehlerflags (0 = kein Fehler) |
| 11–23 | 13 | reserviert | Auf Null |

---

## Design-Entscheidungen

### SLEEP_EVENT Mode

Das Gerät verbringt die meiste Zeit im Tiefschlaf (Deep Sleep). Es wacht auf:
- **Periodisch** alle 12 Stunden (`DEFAULT_WAKE_INTERVAL_S`) für einen Batterie-/Alive-Report
- **Eventgesteuert** über GPIO3-Wake bei Fensteröffnung (steigende Flanke auf HIGH)

Nach dem Senden eines Events oder Reports geht das Gerät sofort wieder in den Tiefschlaf.

### 2x AAA als Batterieprofil

Zwei AAA-Zellen in Reihe liefern im Projektvertrag 2000–3200 mV. `BAT_PROFILE_2X_AAA` definiert die Prozentberechnung für diese Spannungsgrenzen; die ADC-Umrechnung selbst bleibt durch Spannungsteiler und Kalibrierfaktor bestimmt.

### Entprellzeit 35 ms

Mechanische Fensterkontakte (Reed-Kontakte oder Endschalter) prellen typischerweise 5–20 ms. Eine Entprellzeit von 35 ms bietet ausreichend Sicherheit, um Preller zu unterdrücken, ohne die Reaktionszeit unnötig zu verlängern. Die Entprellung ist als nichtblockierender Timer implementiert.

### OPEN_LEVEL_HIGH = 1

`OPEN_LEVEL_HIGH = 1` bedeutet: Wenn der GPIO-Pegel HIGH ist, ist das Fenster als **offen** definiert. Dies ist die Konfiguration für einen Schließer (Normally Open, NO), der bei geöffnetem Fenster schließt. Die Konfiguration erlaubt eine einfache Anpassung an unterschiedliche Kontakttypen ohne Codeänderung.

### GPIO Wake aktiviert

GPIO-Wake ist aktiviert (`WAKE_LEVEL_HIGH = OPEN_LEVEL_HIGH`). Dies erlaubt das Aufwachen aus dem Deep Sleep bei einer steigenden Flanke auf dem Fensterkontakt-Pin. Das Gerät kann so sofort auf Fensteröffnung reagieren, ohne auf den nächsten Timer-Wake zu warten.

### Input Pullup

Der interne Pullup-Widerstand des ESP32 wird verwendet. Dies spart einen externen Pullup-Widerstand und vereinfacht die Beschaltung. Der Pullup zieht den Pegel auf HIGH, solange der Kontakt geschlossen ist (entspricht Fenster zu).

---

## Entprell-Logik (State Machine)

```
                     +-------------+
                     |  Warten     |  raw == lastRaw
                     |  eventReady |  (keine Änderung)
                     +------+------+
                            |
               raw != lastRaw
                            |
                            v
                     +------+------+
                     |  Debounce   |  debounceStart = now
                     |  Timer läuft |  warte 35 ms
                     +------+------+
                            |
               35 ms erreicht?
                            |
                            v
                     +------+------+
                     |  Prüfen     |  newStatus != kontaktOffen?
                     +------+------+
                          |        |
                      Ja  |        |  Nein
                          v        v
              +-----------+    +---------+
              | Update    |    | Ignoriere|  (Pegelrauschen)
              | event=true|    +---------+
              +-----------+
                    |
                    v
              +-----------+
              | Warten    |  (next cycle)
              +-----------+
```

---

## Sleep- / Wake-Verhalten

```
        +-------------------+
        | Deep Sleep        |
        | (CPU aus,        |
        |  ULP/RTC an)      |
        +--------+----------+
                 |
      +----------+----------+
      |                     |
  Timer Wake           GPIO Wake
  (43200 s)            (steigende Flanke GPIO3)
      |                     |
      v                     v
+-----------+        +-----------+
| RTC Boot  |        | RTC Boot  |
+-----------+        +-----------+
      |                     |
      v                     v
+-----------+        +-----------+
| Init I/O  |        | Init I/O  |
| Poll      |        | Poll      |
| Inputs    |        | Inputs    |
+-----------+        +-----------+
      |                     |
      v                     v
+-----------+        +-----------+
| Build     |        | Map Event |
| State     |        | (window   |
| Ch        |        |  opened)  |
+-----------+        +-----------+
      |                     |
      +----------+----------+
                 |
                 v
        +-------------------+
        | ESP-NOW Send      |
        | (Report/Event)    |
        +--------+----------+
                 |
          RX_WINDOW (5 s)
          warten auf ACK?
                 |
                 v
        +-------------------+
        | Deep Sleep        |
        +-------------------+
```

---

## MQTT-Topics (Firmware-Linie)

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/bat_sen_01/meta` | ✅ | Metadaten |
| `smarthome/device/bat_sen_01/availability` | ✅ | Online-Status (online/asleep/offline) |
| `smarthome/device/bat_sen_01/state` | ✅ | Batterie + Fensterzustand |
| `smarthome/device/bat_sen_01/event` | ❌ | Ereignisse (Fenster geöffnet/geschlossen) |
| `smarthome/device/bat_sen_01/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/bat_sen_01/command` | — | Kommandos (get_state, set_config) |
