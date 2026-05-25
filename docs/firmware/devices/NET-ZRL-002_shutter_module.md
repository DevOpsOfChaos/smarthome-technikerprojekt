# NET-ZRL-002 — Shutter Module (Rollladen-Aktor)

**Gerät:** NET-ZRL Shutter Module  
**Device-ID:** `NET-ZRL-002`  
**Firmware-Variante:** `net_zrl_shutter_module`  
**Firmware-Version:** `0.4.0`  
**Bautyp:** ESP32-C3, 2 Relais, 3 Taster, 2 LEDs  
**Protokoll:** ESP-NOW (SmartHome-Protokoll)  
**Stand:** 2026-05-24

---

## Übersicht

Die Firmware steuert einen netzbetriebenen Rollladen/Jalousie-Motor über zwei potentialfreie Relais (Auf/Ab). Sie kommuniziert per ESP-NOW mit einem zentralen Master-Gerät, unterstützt eine vollständige lokale 3-Taster-Bedienung (Auf/Ab/Stop) mit Hold-Gesten (5 s) und verfügt über einen mehrstufigen Kalibrierungsprozess zur automatischen Ermittlung der Fahrzeiten.

---

## 1. Hardware-Konfiguration (DeviceConfig.h)

### Pin-Belegung

| Funktion            | GPIO | Pegel         |
|---------------------|------|---------------|
| Relais Auf          | 10   | Active HIGH   |
| Relais Ab           | 5    | Active HIGH   |
| Taster Auf          | 20   | Active LOW    |
| Taster Ab           | 4    | Active LOW    |
| Taster Stop         | 3    | Active LOW    |
| LED Auf             | 7    | Active HIGH   |
| LED Ab              | 6    | Active HIGH   |

- `BUTTON_ACTIVE_LOW = 0` → Taster schalten auf GND (INPUT_PULLUP)
- Alle Relais- und Taster-Pins sind mittels `static_assert` auf Kollisionen geprüft (kein Pin doppelt belegt).
### Setup-Modus (DOWN-Taster lang drücken)

- **DOWN-Taster 5 Sekunden halten**: Aktiviert den Setup-Modus.
- **Setup-Modus**: Das Gerät startet einen WiFi-Access-Point.
  SSID = Geräte-ID (z.B. `NET-ZRL-002`), Passwort = `smarthome`.
  Über das Webinterface (`192.168.4.1`) können Master-MAC und
  Konfigurationswerte eingestellt werden.
- **Während Setup**: Sensorik und Funk-Kommunikation pausieren.
- **Status-LED**: Beide LEDs blinken während des Setup-Modus schnell (200 ms Intervall).
- **Timeout**: Nach 5 Minuten ohne Speichern wird der Setup-Modus automatisch
  beendet und das Gerät startet neu.
- **Normale Funktion**: Kurzes Drücken (< 5 s) des DOWN-Tasters ohne Halten
  löst die normale Ab-Fahrt aus.

### Devicespezifische Konstanten

| Konstante                     | Wert    | Beschreibung                         |
|-------------------------------|---------|--------------------------------------|
| `DEFAULT_ESTIMATED_TRAVEL_TIME_MS` | 100.000 ms | Fallback-Fahrzeit ohne Kalibrierung |
| `RELAY_DEAD_TIME_MS`          | 300 ms  | Mindestpause zwischen Richtungswechsel |
| `LED_BLINK_INTERVAL_MS`       | 300 ms  | Blink-Intervall im Normalmodus       |
| `LED_SUCCESS_INTERVAL_MS`     | 180 ms  | Blink-Intervall für Bestätigungen    |
| `LED_ACK_DURATION_MS`         | 180 ms  | Dauer der ACK-LED (beide an)         |
| `HOLD_MS`                     | 5.000 ms| Haltezeit für Gesten                 |
| `BUTTON_DEBOUNCE_MS`          | 35 ms   | Entprellzeit                         |
| `BUTTON_POLL_MS`              | 20 ms   | Taster-Abfrageintervall              |
| `LOOP_DELAY_MS`               | 10 ms   | Arduino-loop()-Verzögerung           |
| `MIN_TRAVEL_TIME_MS`          | 1.000 ms| Minimal gültige Fahrzeit             |
| `MAX_TRAVEL_TIME_MS`          | 180.000 ms| Maximal gültige Fahrzeit           |
| `WDT_TIMEOUT_S`               | 10 s    | Watchdog-Timeout                     |

---

## 2. Provisioning & Setup-Portal

### Setup-Zugang

- **SSID:** Dynamisch aus `DEVICE_ID` (z. B. `NET-ZRL-002`)
- **Passwort:** `net-zrl-setup`
- **Kanal:** WLAN-Kanal 1 (Setup-AP)

### Storage

- **Namespace:** `net_zrl`
- **Node-Basis-Key:** `node_basis_v1`
- **Device-Blob-Key:** `net_zrl_v1`
- **Magic:** `0x5A524C32` (`"ZRL2"`), **Version:** `1`

### Persistierte Daten (NetZrlPersistedSetupData)

| Feld                        | Typ      | Beschreibung                              |
|-----------------------------|----------|-------------------------------------------|
| `magic`                     | uint32_t | Validierungs-Magic                        |
| `version`                   | uint16_t | Strukturversion                           |
| `travelTimeUpMs`            | uint32_t | Kalibrierte Fahrzeit Auf (0 = ungültig)   |
| `travelTimeDownMs`          | uint32_t | Kalibrierte Fahrzeit Ab (0 = ungültig)    |
| `defaultEstimatedTravelTimeMs` | uint32_t | Fallback-Fahrzeit                       |
| `relayUpUsesRelayA`         | uint8_t  | 1 = Relais A = Auf, 0 = Relais B = Auf   |

### Web-Provisioning (NetZrlProvisioningHandler)

Die Klasse `NetZrlProvisioningHandler` erweitert das gemeinsame `DeviceProvisioningHandler`-Interface und stellt im Setup-Webportal bereit:

- **travel_time_up_ms / travel_time_down_ms** — Kalibrierte Fahrzeiten (optionales Feld, leer = unverändert)
- **default_estimated_travel_time_ms** — Fallback-Fahrzeit (Pflichtfeld)
- **relay_up_mapping** — Relais-Zuordnung für Auf (relay_a / relay_b)
- **Aktion: reset_calibration** — Löscht Kalibrierwerte mit Rollback

#### Rollback-Mechanismus

1. `holeSetupSnapshot()` erfasst den gesamten aktuellen Zustand (Basis + Device)
2. Änderung wird angewandt
3. `speicherePersistenzMitRollback()` versucht Speichern
4. Bei Fehler: `wendeSetupSnapshotAn()` stellt den Vorzustand vollständig wieder her

---

## 3. Laufzeitzustand (RuntimeState)

Die zentrale Struktur `RuntimeState` (~70 Felder) hält den gesamten Gerätezustand:

### Bewegung
- `coverState` — `Stopped(0)` / `Moving(1)`
- `coverDirection` — `None(0)` / `Up(1)` / `Down(2)`
- `relayAActive`, `relayBActive` — Aktiver Relais-Zustand
- `letzteRelaisWechselMs` — Zeitstempel für Dead-Time
- `relayUpUsesRelayA` — Relais-Mapping
- `coverPosition` — Geschätzte Position (0–100, 255 = unbekannt)
- `movementStartedAtMs`, `movementAutoStopMs` — Fahrt-Timing
- `movementTargetPosition`, `movementTargetsEndPosition`, `movementTargetsIntermediatePosition`

### Kalibrierung
- `calibrationMode`, `calibrationPhase` — Kalibrierungs-State-Machine
- `travelTimeUpMs`, `travelTimeDownMs` — Kalibrierte Fahrzeiten
- `candidateTravelTimeUpMs`, `candidateTravelTimeDownMs` — Messkandidaten
- `isCalibrated` — Beide Fahrzeiten gültig
- `defaultEstimatedTravelTimeMs` — Fallback

### Kommunikation
- `masterMacValid`, `masterMac[6]` — Provisionierte Master-MAC
- `masterBound` — HELLO_ACK vom Master empfangen
- `funkBereit` — ESP-NOW initialisiert
- `naechsteSeq` — Nächste Sequenznummer
- `letztesHelloMs`, `letzterHeartbeatMs`, `letzterStateMs`

### Taster-Entprellung
- `{up,down,stop}ButtonStableActive` — Entprellter Zustand
- `{up,down,stop}ButtonRawActive` — Rohwert
- `{up,down,stop}ButtonRawChangedAtMs` — Zeit letzter Änderung
- `{up,down,stop}PressedAtMs` — Zeitstempel des Press-Ereignisses
- `{up,down,stop}HoldConsumed` — Hold-Geste bereits verarbeitet

### LEDs
- `ledMode` — Aktueller Modus (Off/BothBlink/UpBlink/DownBlink/UpOn/DownOn)
- `ledModeAfterAck` — Wiederherzustellender Modus nach ACK
- `ledBlinkState`, `ledLastTickMs` — Blink-Timing
- `ledAckActive`, `ledAckStartedAtMs` — ACK-LED-Phase
- `successBlinkToggleCount` — Verbleibende Success-Blink-Wechsel

### Ausstehende Aktionen
- `pendingAction` — `None` / `SetupEnter` / `FactoryReset`
- `pendingActionBlinkToggleCount` — Verbleibende Blink-Wechsel vor Ausführung

---

## 4. Hilfsfunktionen

### Logging
- **`logf(level, format, ...)`** — Formatiertes Debug-Log (nur bei `DEBUG_ENABLED`)
- **`provisioningLog(level, message)`** — Provisioning-spezifisches Logging
- **`loggeButtonKante(name, active, heldMs)`** — Taster-Ereignis-Log

### Enum-zu-Text
- `toText(CoverState)` → `"moving"` / `"stopped"`
- `toText(CoverDirection)` → `"up"` / `"down"` / `"null"`
- `toText(CalibrationPhase)` → Phasenname in snake_case
- `toText(PendingAction)` → `"setup_enter"` / `"factory_reset"` / `"none"`

### Validierung & Sanitization
- **`isTravelTimeValid(valueMs)`** — Prüft 1.000 ≤ value ≤ 180.000 ms
- **`isSendIntervalValid(valueS)`** — Prüft 10 ≤ value ≤ 65.535 s
- **`sanitizeEstimatedTravelTime(valueMs)`** — Fallback auf `DEFAULT_ESTIMATED_TRAVEL_TIME_MS`
- **`sanitizeStatusSendInterval(valueS)`** — Fallback auf 60 s

### MAC-Adressen
- **`clearStoredMasterMac()`** / **`setStoredMasterMac(mac)`** — Master-MAC setzen/löschen
- **`formatMacText(mac, isValid, buffer, size)`** — MAC als `AA:BB:CC:DD:EE:FF`
- **`parseMacText(text, outMac)`** — Text in MAC parsen
- **`istBroadcastMac(mac)`** — Prüft auf FF:FF:FF:FF:FF:FF
- **`senderIstBekannterMaster(senderMac)`** — Vergleich mit provisionierter MAC

### Positionslogik
- **`begrenzePosition(position)`** — Clamp auf 0–100
- **`istCoverPositionBekannt(position)`** — true wenn 0 ≤ position ≤ 100
- **`setzeCoverPositionUnbekannt()`** — Setzt Position auf 255
- **`coverPositionFuerPayload()`** — Position (0–100) oder 255 für Protokoll

### Protokoll-Hilfen
- **`coverStateCodeAusRuntime()`** — `SH_COVER_STATE_STOPPED` / `MOVING_UP` / `MOVING_DOWN`
- **`darfFunkAktivSein()`** — true außer im Setup-Modus
- **`baueSensorMask(target, size)`** — `"XXXXXXXXXX"`
- **`baueInputMask(target, size)`** — `"BTN3X"`
- **`holeHelloZielMac()`** — Master-MAC oder Broadcast

---

## 5. Kommunikation (ESP-NOW)

### Senden

#### stellePeerSicher(mac)
Registriert eine MAC als ESP-NOW-Peer. Broadcast-MAC und gültige Unicast-MACs werden akzeptiert. Bereits existierende Peers werden nicht doppelt registriert.

#### sendePaketMitOptionen(zielMac, msgType, payload, payloadLen, label, flags, verwendeteSeq)
1. Prüft `funkBereit`, Ziel-MAC und Payload-Länge
2. Ruft `stellePeerSicher()` auf
3. Baut `MsgHeader` mit Sequenznummer, Typ, Flags, Länge
4. Kopiert Payload hinter den Header
5. Berechnet und setzt CRC via `finalizePacketCrc()`
6. Sendet per `esp_now_send()`

#### sendePaket(zielMac, msgType, payload, payloadLen, label)
Wrapper ohne Flags/Seq-Rückgabe.

#### sendePaketMitRetry(zielMac, msgType, payload, payloadLen, label)
- Wiederholt bis zu 2× (**`NET_ZRL_ESPNOW_RETRY_COUNT`**) bei Fehler
- **50 ms Pause** zwischen Versuchen (**`NET_ZRL_ESPNOW_RETRY_DELAY_MS`**)
- **Alle Versuche verwenden dieselbe Sequenznummer** → der Master erkennt Duplikate
- Sequenzzähler wird nur bei Erfolg oder nach allen Fehlversuchen erhöht

#### sendeAck(zielMac, ackSeq, ackMsgType, status)
Sendet `SH_MSG_ACK` mit der quittierten Sequenz, Message-Typ und Statuscode.

### Empfangen

#### verarbeiteEspNowPaket(senderMac, data, len)
1. Validiert CRC via `hasValidPacketCrc()` — ungültige Pakete werden verworfen
2. Dispatched per `switch(msg_type)` an:
   - `SH_MSG_HELLO_ACK` → `verarbeiteHelloAck()`
   - `SH_MSG_CMD` → `verarbeiteCmd()`
   - `SH_MSG_CFG` → `verarbeiteCfg()`

#### Duplikaterkennung
Wenn `SH_FLAG_ACK_REQUEST` gesetzt ist und die Sequenznummer bereits quittiert wurde, wird der gespeicherte ACK erneut gesendet (ohne erneute Verarbeitung).

### Callbacks
- **`onEspNowReceive()`** — Versioniert für ESP_ARDUINO_VERSION_MAJOR ≥ 3 vs. Legacy
- **`onEspNowSend()`** — Loggt Fehler bei `ESP_NOW_SEND_SUCCESS`

### Funk-Initialisierung

#### initialisiereFunk()
1. `WiFi.mode(WIFI_STA)`, `WiFi.disconnect()`, `WiFi.setSleep(false)`
2. Setzt WLAN-Kanal (Standard: 6)
3. `esp_now_init()` mit bis zu **5 Fehlschlagversuchen** → danach `ESP.restart()`
4. Registriert Send/Receive-Callbacks
5. Fügt Broadcast-Peer hinzu
6. Fügt Master-MAC-Peer hinzu (falls provisioniert)
7. Setzt `funkBereit = true`

### Kommunikations-Tick

#### tickKommunikation()
Wird jeden Loop-Aufruf ausgeführt:
1. Wenn nicht `funkBereit` → `initialisiereFunk()` erneut versuchen
2. Wenn `!masterBound` → HELLO alle `HELLO_RETRY_INTERVAL_MS` (5 s) senden
3. Wenn `masterBound`:
   - HELLO-Reannounce alle `HELLO_REANNOUNCE_INTERVAL_MS`
   - HEARTBEAT alle `HEARTBEAT_INTERVAL_MS` (20 s)
   - STATE bei dirty-Flag (`stateReportOffen`) oder Intervall-Ablauf

### Protokoll-Verarbeitung

#### verarbeiteHelloAck(senderMac, payload)
1. Prüft `ack_status == SH_ACK_OK`
2. `uebernehmeMasterMacNachHelloAck()` — bestätigt, dass Sender der provisionierte Master ist
3. Setzt `masterBound = true`
4. Sendet STATE als Antwort

#### verarbeiteCmd(senderMac, header, payload)
1. **Nur der provisionierte Master** darf CMD senden
2. Duplikaterkennung über Sequenznummer
3. Ruft `verarbeiteCmdTyp()` auf → switch über `payload.cmd_type`:
   - **`SH_CMD_STATE_REQUEST`** — STATE sofort senden
   - **`SH_CMD_HELLO_REQUEST`** — HELLO senden
   - **`SH_CMD_COVER`** — Cover-Kommandos:
     - `SH_COVER_CMD_OPEN` → `starteNormaleFahrtNachOben()`
     - `SH_COVER_CMD_CLOSE` → `starteNormaleFahrtNachUnten()`
     - `SH_COVER_CMD_STOP` → `stoppeFahrtMitEvent()`
     - `SH_COVER_CMD_SET_POSITION` → `verarbeitePositionCmd()` (kalibriert → `startePositionsfahrt()`, unkalibriert → `starteEndlagenfahrtOhneKalibrierung()`)
4. Blockiert während Setup-Modus, Kalibrierung oder ausstehender Aktion
5. Sendet ACK wenn angefordert

#### verarbeiteCfg(senderMac, header, payload)
- Nur `SH_CFG_REPORT_INTERVAL_S` wird unterstützt
- Wendet Wert mit Rollback an
- Sendet STATE nach erfolgreicher Übernahme

---

## 6. Hardware-Steuerung

### Taster

#### leseButtonAktiv(pin)
Liest den Pin und invertiert bei `BUTTON_ACTIVE_LOW`.

#### entprelleButton(rawActive, lastRawActive, changedAtMs, previousStableActive, jetztMs)
- Bei Änderung des Rohwerts: Zeitstempel speichern
- Erst nach **stabilen 35 ms** (`BUTTON_DEBOUNCE_MS`) wird der neue Wert übernommen
- Gibt den entprellten stabilen Zustand zurück

#### pollButtons()
Alle 20 ms (`BUTTON_POLL_MS`):
1. Liest alle drei Taster-Rohwerte
2. Entprellt jeden separat
3. Ruft die Handler auf: `behandleStopButton()` → `behandleUpButton()` → `behandleDownButton()`

### Relais

#### schreibePin(pin, active, activeHigh)
Setzt einen GPIO unter Berücksichtigung der Polarität (active HIGH/LOW).

#### setzeLedPins(upOn, downOn)
Setzt beide LED-Pins.

#### setzeRelaisNeutral(grund)
- Schaltet **beide Relais AUS** (sicherer Ruhezustand)
- Aktualisiert `letzteRelaisWechselMs`

#### setzeRelaisFuerRichtung(direction, grund)
**Sicherheitskritisch!** Vier-Schritt-Sequenz (Break-Before-Make):

1. Wenn eines der Relais aktiv ist → `setzeRelaisNeutral("break-before-make")`
2. **Dead-Time einhalten** — falls seit letztem Wechsel weniger als `RELAY_DEAD_TIME_MS` (300 ms) vergangen sind → `delay(restMs)`
3. **Beide Relais AUS** (Break — sicherer Zustand)
4. **Ziel-Relais EIN** (Make) — über `pinFuerRichtung()` / `activeHighFuerRichtung()` gemäß `relayUpUsesRelayA`

#### pinFuerRichtung(direction) / activeHighFuerRichtung(direction)
Berücksichtigt `relayUpUsesRelayA`:
- `direction == Up` → liefert Relay A wenn Mapping = Relay A, sonst Relay B
- `direction == Down` → liefert das jeweils andere Relais

---

## 7. Bewegungssteuerung

### Fahrt-Start

#### starteFahrt(direction, grund, autoStopMs, targetsEnd)
1. Setzt `coverDirection`, `coverState = Moving`, `movementStartedAtMs`
2. `movementStartPosition` = aktuelle Position (0 wenn unbekannt)
3. Setzt Auto-Stop-Dauer
4. **Ohne Kalibrierung:** Position = `COVER_POSITION_UNBEKANNT` (Fahrt verlässt letzte sichere Endlage)
5. Ruft `setzeRelaisFuerRichtung()` auf
6. Setzt `stateReportOffen`

#### starteNormaleFahrtNachOben(grund, trigger)
- Auto-Stop = `travelTimeUpMs * 120%` (20 % Sicherheitsaufschlag)
- Ohne Kalibrierung: Auto-Stop = `defaultEstimatedTravelTimeMs`
- Sendet `COVER_UP`-Event, setzt LED auf `UpOn`

#### starteNormaleFahrtNachUnten(grund, trigger)
- Auto-Stop = `travelTimeDownMs * 120%`
- Sendet `COVER_DOWN`-Event, setzt LED auf `DownOn`

#### startePositionsfahrt(zielPosition, grund)
**Nur mit Kalibrierung möglich.**
1. Berechnet Distanz: `|zielPosition - aktuellePosition|`
2. Fahrzeit = `(fahrzeitMs * deltaProzent) / 100` (mindestens 1 ms)
3. Startet proportionale Fahrt, setzt `movementTargetsIntermediatePosition = true`

#### starteEndlagenfahrtOhneKalibrierung(zielPosition, grund, trigger)
- Nur 0 oder 100 werden akzeptiert (Zwischenpositionen ohne Kalibrierung gesperrt)
- Leitet auf `starteNormaleFahrt*()` weiter

### Auto-Stop und Position

#### fahrzeitFuerRichtung(direction)
Liefert `travelTimeUpMs` oder `travelTimeDownMs`.

#### aktualisierePositionsschaetzung(jetztMs)
Nur bei aktiver Fahrt und Kalibrierung:
- `delta = (elapsedMs * 100) / fahrzeitMs`
- `nextPosition = startPosition + (delta * direction)`
- Bei Auf: Position steigt, bei Ab: Position sinkt
- Ergebnis wird auf 0–100 begrenzt

#### verarbeiteBewegungsTimeouts()
1. Prüft ob `movementAutoStopMs` erreicht
2. **Kalibrierung MovingToTop:** Stopp → Phase = `WaitForDownStart`, LED = `DownBlink`
3. **Kalibrierung MeasuringDown/Up:** Stopp → `beendeKalibriermodus("timeout", false)` (Fehler)
4. **Normale Fahrt:** Position setzen, Relais neutral, LED-Reset

### Fahrt-Stopp

#### stoppeFahrt(grund, unkalibrierteEndlageBestaetigt)
1. `aktualisierePositionsschaetzung()`
2. Ohne Kalibrierung und ohne bestätigte Endlage → Position = unbekannt
3. `setzeRelaisNeutral()`
4. Alle Bewegungsfelder zurücksetzen
5. `stateReportOffen = true`

#### stoppeFahrtMitEvent(grund, trigger)
Stoppt und sendet `COVER_STOP`-Event.

#### setzePositionAufEndlage(direction)
Setzt Position auf 100 (Auf) oder 0 (Ab) — nur nach vollständiger Auto-Stop-Fahrt bei erwarteter Endlage.

### Relais-Mapping
`relayUpUsesRelayA` (persistiert) bestimmt, welches Relais für Auf verwendet wird:
- `true` → Relais A (GPIO 10) = Auf, Relais B (GPIO 5) = Ab
- `false` → Relais B (GPIO 5) = Auf, Relais A (GPIO 10) = Ab

---

## 8. Kalibrierung

### Zustände

#### berechneKalibrierstatus()
Setzt `isCalibrated = true` nur wenn **beide** Fahrzeiten gültig sind (1.000–180.000 ms). Bei `false` → Position = unbekannt.

#### loescheKalibrierungszustandImRuntime()
Setzt alle Fahrzeiten auf 0, `isCalibrated = false`, Position unbekannt.

### State Machine (5-Phasen Kalibrierung)

#### starteKalibriermodus()
Ausgelöst durch Stop-Taster 5 s halten (bei stehendem Rollladen, ohne Setup-Modus):

```
 Idle (0)
   │
   ▼  [Stop-Hold 5s]
 MovingToTop (1)       ← Fahrt mit defaultEstimatedTravelTimeMs nach oben
   │
   ▼  [Auto-Stop erreicht]
 WaitForDownStart (2)  ← Wartet auf Up- oder Stop-Taster (LED: DownBlink)
   │
   ├─ Stop-Taster drücken → beendeKalibriermodus(false) [Abbruch]
   │
   └─ Down-Taster drücken
         │
         ▼
     MeasuringDown (3)  ← Fahrt nach unten mit MAX_TRAVEL_TIME_MS
         │               ← Neuer Stop-Taster drücken = Messung übernehmen
         ▼               ← Auto-Stop (MAX) = Fehler, Abbruch
   WaitForUpStart (4)   ← Wartet auf Down- oder Stop-Taster (LED: UpBlink)
         │
         ├─ Stop-Taster drücken → beendeKalibriermodus(false) [Abbruch]
         │
         └─ Up-Taster drücken
               │
               ▼
           MeasuringUp (5)  ← Fahrt nach oben mit MAX_TRAVEL_TIME_MS
               │             ← Neuer Stop-Taster drücken = Messung übernehmen
               ▼             ← Auto-Stop (MAX) = Fehler, Abbruch
         SuccessBlink (6)   ← 3× Blinken (180 ms Intervall)
               │
               ▼
         beendeKalibriermodus("kalibrierung erfolgreich", true)
               │
               ▼
         Fahrzeiten gespeichert, isCalibrated = true
```

#### uebernehmeKalibrierMessung(direction)
- Wird beim Stop-Taster-Druck während MeasuringDown oder MeasuringUp aufgerufen
- Misst `elapsedMs = millis() - movementStartedAtMs`
- Ungültige Zeit → Abbruch
- **Down gemessen** → `candidateTravelTimeDownMs`, Phase = `WaitForUpStart`, LED = `UpBlink`
- **Up gemessen** → `candidateTravelTimeUpMs`, Phase = `SuccessBlink`, startet 3× Bestätigungsblinken

#### beendeKalibriermodus(grund, messwerteUebernehmen)
1. Stoppt Fahrt
2. Wenn `messwerteUebernehmen` und beide Kandidaten gültig → übernimmt die gemessenen Fahrzeiten, setzt Position auf 100
3. Bereinigt Kalibrierungszustand
4. **Speichert mit Rollback**
5. Sendet `COVER_CALIB_DONE`-Event bei Erfolg

---

## 9. LED-Tick

### tickLeds() — Prioritäten (absteigend)

```
Prio 1: ACK-LED aktiv?
   ├─ JA → Beide LEDs AN für 180 ms (LED_ACK_DURATION_MS)
   │        Danach: ledMode = ledModeAfterAck
   │        └─ return
   │
Prio 2: Ausstehende Aktion (PendingAction)?
   ├─ JA → Blink-Sequenz abarbeiten (180 ms Intervall)
   │        Wenn Blinks abgearbeitet:
   │          ├─ SetupEnter → enterSetupMode()
   │          └─ FactoryReset → fuehreFactoryResetAus()
   │        └─ return
   │
Prio 3: CalibrationPhase == SuccessBlink?
   ├─ JA → 3× Blinken (180 ms Intervall)
   │        Danach: beendeKalibriermodus("kalibrierung erfolgreich", true)
   │        └─ return
   │
Prio 4: Normaler LED-Mode
   ├─ BothBlink → beide LEDs blinken synchron (300 ms)
   ├─ UpBlink   → nur Auf-LED blinkt
   ├─ DownBlink → nur Ab-LED blinkt
   ├─ UpOn      → Auf-LED dauerhaft an
   ├─ DownOn    → Ab-LED dauerhaft an
   └─ Off       → beide aus
```

### Steuerung

- **`bestaetigeMitBeidenLeds(nextMode)`** — Schaltet beide LEDs sofort an, startet ACK-Timer, merkt `ledModeAfterAck`
- **`setzeLedNachNormalemStop()`** — Setzt LEDs zurück (außer bei Kalibrierung/Setup/Aktion)
- **`starteAusstehendeAktion(action, blinkPulses)`** — Startet Blink-Sequenz (2× Pulses = Anzahl der Toggles)
- **`verwerfeAusstehendeAktion(grund)`** — Bricht Blink-Sequenz ab, setzt LEDs zurück

### LED-Modi

| Modus      | Beschreibung                           | Typische Verwendung               |
|------------|----------------------------------------|-----------------------------------|
| Off        | Beide LEDs aus                         | Ruhezustand                       |
| BothBlink  | Beide blinken synchron                 | Kalibrierung aktiv                |
| UpBlink    | Nur Auf-LED blinkt                     | Warte auf Auf-Messung (Kalibrierung Phase 4) |
| DownBlink  | Nur Ab-LED blinkt                      | Warte auf Ab-Messung (Kalibrierung Phase 2) |
| UpOn       | Auf-LED dauerhaft an                   | Fahrt nach oben                   |
| DownOn     | Ab-LED dauerhaft an                    | Fahrt nach unten                  |

---

## 10. Taster-Handler mit Hold-Gesten

### Hold-Erkennung

Alle drei Taster unterstützen **5-Sekunden-Hold-Gesten** (`HOLD_MS = 5000`). Die Erkennung erfolgt in `pollButtons()` alle 20 ms.

#### Stop-Taster

| Aktion               | Bedingung                                    | Ergebnis                              |
|----------------------|----------------------------------------------|---------------------------------------|
| Kurzer Druck (fallend) | Während Bewegung, Kalibrierungs-Messung aktiv | `uebernehmeKalibrierMessung()`       |
| Kurzer Druck (fallend) | Während Bewegung                             | `stoppeFahrtMitEvent()`               |
| Kurzer Druck (fallend) | Während Kalibrierung                         | `beendeKalibriermodus()` (Abbruch)    |
| Kurzer Druck (fallend) | Während Setup-Modus                          | `exitSetupMode()`                     |
| Kurzer Druck (fallend) | Factory-Reset-Blink aktiv                    | `verwerfeAusstehendeAktion()`         |
| **5 s halten**        | Stehend, keine Kalibrierung/Setup/Aktion     | `starteKalibriermodus()`              |

#### Auf-Taster

| Aktion               | Bedingung                                    | Ergebnis                              |
|----------------------|----------------------------------------------|---------------------------------------|
| Kurzer Druck (fallend) | Kalibrierung Phase `WaitForUpStart`         | Startet Auf-Messfahrt                 |
| Loslassen (< 5 s)     | Stehend, keine Kalibrierung/Setup/Aktion     | `starteNormaleFahrtNachOben()`        |
| **5 s halten**        | Stehend, keine Kalibrierung/Setup/Aktion     | `PendingAction::FactoryReset` (10× Blinken) |

#### Ab-Taster

| Aktion               | Bedingung                                    | Ergebnis                              |
|----------------------|----------------------------------------------|---------------------------------------|
| Kurzer Druck (fallend) | Kalibrierung Phase `WaitForDownStart`       | Startet Ab-Messfahrt                  |
| Loslassen (< 5 s)     | Stehend, keine Kalibrierung/Setup/Aktion     | `starteNormaleFahrtNachUnten()`       |
| **5 s halten**        | Stehend, keine Kalibrierung/Setup/Aktion     | `PendingAction::SetupEnter` (3× Blinken) |

### PendingAction-System (Hold-Gesten)

Nach 5 s Halten wird eine **ausstehende Aktion** mit Blink-Bestätigung gestartet:

1. `starteAusstehendeAktion(action, blinkPulses)` setzt `pendingAction`, Blink-Zähler
2. Der LED-Tick blinkt beide LEDs im 180-ms-Intervall (`LED_SUCCESS_INTERVAL_MS`)
3. Nach Ablauf aller Blinks wird die Aktion ausgeführt:
   - **SetupEnter** (3 Pulse = 6 Toggles) → `enterSetupMode()`
   - **FactoryReset** (10 Pulse = 20 Toggles) → `fuehreFactoryResetAus()`
4. Stop-Taster während Blinken → `verwerfeAusstehendeAktion()` (Abbruch)

---

## 11. Serielle Konsole

**Baudrate:** 115.200

### Befehle

| Befehl                                       | Beschreibung                          |
|----------------------------------------------|---------------------------------------|
| `status`                                     | Zeigt alle Zustandsfelder             |
| `help`                                       | Zeigt Hilfe                           |
| `up` / `down` / `stop`                       | Bewegung (bei Kalibrierung: Messung)  |
| `cal abort`                                  | Kalibrierung abbrechen                |
| `setup show`                                 | Zeigt Status (Alias für status)       |
| `setup enter`                                | Setup-Modus aktivieren                |
| `setup exit`                                 | Setup-Modus beenden                   |
| `setup set master_mac AA:BB:CC:DD:EE:FF`     | Master-MAC setzen                     |
| `setup set tt_up_ms <ms>`                    | Fahrzeit Auf setzen                   |
| `setup set tt_down_ms <ms>`                  | Fahrzeit Ab setzen                    |
| `setup set default_ms <ms>`                  | Fallback-Fahrzeit setzen              |
| `setup set relay_up relay_a|relay_b`         | Relais-Mapping setzen                 |
| `setup set status_interval_s <s>`            | STATE-Sendeintervall setzen           |
| `setup set sensor_interval_s <s>`            | Sensor-Sendeintervall setzen          |
| `setup reset_calibration`                    | Kalibrierung löschen                  |

**Puffer:** 128 Byte, Zeilenweise via `\n`, `\r` wird ignoriert.

---

## 12. Persistenz mit Rollback

### Speicherkette

Jeder persistenzrelevante Vorgang folgt dem gleichen Muster:

1. **Snapshot vorher** `holeSetupSnapshot(basisSnapshot, deviceSnapshot)`
2. **Änderung anwenden** (Runtime-Zustand ändern)
3. **Speicherversuch** `speicherePersistenzMitRollback(basisSnapshot, deviceSnapshot)`
4. Bei Erfolg → fertig
5. Bei Fehler → `wendeSetupSnapshotAn(basisSnapshot, deviceSnapshot)` stellt **alle** Felder wieder her

### Beteiligte Funktionen

- **`holeNetZrlSetupSnapshot(snapshot)`** — Erfasst `travelTimeUpMs`, `travelTimeDownMs`, `defaultEstimatedTravelTimeMs`, `relayUpUsesRelayA`
- **`wendeNetZrlSetupSnapshotAn(snapshot)`** — Stellt Device-Felder wieder her, ruft `berechneKalibrierstatus()`
- **`baueNetZrlPersistenzdatenAusRuntime()`** — Baut `NetZrlPersistedSetupData` mit Magic/Version
- **`netZrlPersistenzdatenSindGleich(a, b)`** — Vermeidet unnötige Schreibvorgänge
- **`netZrlPersistenzdatenGueltig(data)`** — Prüft Magic und Version

### Persistenz im ProvisioningHandler

- **`loadDeviceSettings(prefs)`** — Liest Blob, validiert Magic, wendet an
- **`saveDeviceSettings(prefs)`** — Überschreibt nur bei geänderten Werten
- **`clearDeviceSettings(prefs)`** — Entfernt Blob

---

## 13. Factory Reset

### fuehreFactoryResetAus()

1. Snapshot für Rollback erstellen (Basis + Device)
2. Fahrt stoppen, Kalibrierung löschen
3. `nodeProvisioning.applyDefaultBasisValues()` — Basiswerte zurücksetzen
4. `netZrlProvisioningHandler.loadDeviceDefaults()` — Device-Standardwerte
5. `clearStoredSettings()` — NVS-Speicher löschen
6. Bei Fehler → Rollback
7. Bei Erfolg → `enterSetupMode()` → Gerät startet im Setup-Portal

---

## 14. Setup / Loop-Flow

### setup()

```
1. Serial.begin(115200), delay(150)
2. RuntimeState = {} initialisieren
   ├─ coverState = Stopped, coverDirection = None
   ├─ coverPosition = COVER_POSITION_UNBEKANNT (255)
   ├─ calibrationPhase = Idle, ledMode = Off
   ├─ defaultEstimatedTravelTimeMs = 100000
   ├─ relayUpUsesRelayA = true
   ├─ statusSendIntervalS = 60
   └─ stateReportOffen = true (direkt nach Boot STATE senden)
3. GPIOs initialisieren
   ├─ Relais (OUTPUT) → beide AUS (Neutral)
   ├─ LEDs (OUTPUT) → beide AUS
   └─ Taster (INPUT_PULLUP oder INPUT_PULLDOWN)
4. Watchdog aktivieren (10 s)
5. Provisioning-Basis starten
   ├─ nodeProvisioning.begin() mit Config, Callbacks
   └─ MAC, Sendeintervalle, Setup-Modus werden geladen
6. Werte sanitizen
7. Wenn KEINE Master-MAC persistiert:
   ├─ enterSetupMode() → Setup-Portal starten
   └─ return
8. initialisiereFunk() → ESP-NOW starten
9. Log: Startmeldung, Hilfe, Status
```

### loop()

```
1. esp_task_wdt_reset()          ─ Watchdog füttern
2. verarbeiteSerielleBefehle()    ─ Serielle Eingabe parsen
3. pollButtons()                  ─ Taster alle 20 ms mit Entprellung
4. verarbeiteBewegungsTimeouts()  ─ Auto-Stop prüfen
5. aktualisierePositionsschaetzung()  ─ Position während Fahrt schätzen (nur kalibriert)
6. nodeProvisioning.update()      ─ Provisioning-Webserver/Setup-AP
7. tickKommunikation()            ─ HELLO, HEARTBEAT, STATE senden
8. tickLeds()                     ─ LED-Zustand aktualisieren
9. delay(10 ms)                   ─ LOOP_DELAY_MS
```

---

## 15. Sicherheitsmechanismen

### Break-Before-Make (Relais)
- **300 ms Dead-Time** zwischen Richtungswechseln
- Beide Relais werden ZUERST ausgeschaltet, DANN das Ziel-Relais eingeschaltet
- Verhindert Kurzschluss über den Motor (wenn beide Relais gleichzeitig schalten)

### Auto-Stop
- Jede Fahrt hat eine berechnete Maximaldauer (`movementAutoStopMs`)
- Kalibriert: Fahrzeit × 120 %
- Unkalibriert: `defaultEstimatedTravelTimeMs`
- Bei Timeout: Fahrt wird sofort gestoppt

### Watchdog
- **10 s Timeout** (`NET_ZRL_WDT_TIMEOUT_S`)
- Wird in jedem Loop-Durchlauf via `esp_task_wdt_reset()` zurückgesetzt
- Bei Auslösung: Panic-Reset des ESP32

### Positionsintegrität ohne Kalibrierung
- Zwischenpositionen (1–99 %) sind ohne Kalibrierung **gesperrt**
- Nur Endlagen (0 % / 100 %) sind durch vollständige Auto-Stop-Fahrten erreichbar
- Position wird nach unkalibrierter Fahrt auf `COVER_POSITION_UNBEKANNT` (255) gesetzt, es sei denn, die Endlage wurde durch Auto-Stop bestätigt

### Master-Validierung
- Nur die provisionierte Master-MAC darf Kommandos senden
- HELLO_ACK wird nur akzeptiert, wenn der Sender der provisionierte Master ist
- Duplikaterkennung über Sequenznummern

### Rollback bei Persistenzfehlern
Jeder persistenzrelevante Vorgang sichert vorher den Zustand und stellt ihn bei Fehlern vollständig wieder her.

---

## 16. Wichtige Protokoll-Payloads

### State-Report (ZrlConfigStateReportPayload, 25 Bytes)

| Feld              | Größe | Beschreibung                          |
|-------------------|-------|---------------------------------------|
| `node_id`         | 16 B  | Device-ID                             |
| `relay_1`         | 1 B   | Relais-1-Zustand                      |
| `relay_2`         | 1 B   | Relais-2-Zustand                      |
| `cover_mode`      | 1 B   | Cover-Modus (immer 1)                 |
| `cover_state`     | 1 B   | `STOPPED`/`MOVING_UP`/`MOVING_DOWN`   |
| `cover_position`  | 1 B   | 0–100 oder 255 (unbekannt)            |
| `cover_calibrated`| 1 B   | 0/1                                   |
| `fault`           | 1 B   | Fehlerstatus (immer 0)                |
| `report_interval_s`| 2 B  | Sendeintervall in Sekunden            |

### Events

| Event                      | Auslöser                        |
|----------------------------|----------------------------------|
| `COVER_UP`                 | Normale Auf-Fahrt gestartet      |
| `COVER_DOWN`               | Normale Ab-Fahrt gestartet       |
| `COVER_STOP`               | Fahrt gestoppt                   |
| `COVER_CALIB_START`        | Kalibrierung gestartet           |
| `COVER_CALIB_DONE`         | Kalibrierung erfolgreich         |

### HELLO-Payload

| Feld                | Beschreibung                              |
|---------------------|-------------------------------------------|
| `device_id`         | `"NET-ZRL-002"`                           |
| `device_name`       | `"NET-ZRL Shutter"`                       |
| `device_class`      | `SH_CLASS_NET_ZRL`                        |
| `caps_hi / caps_lo` | `RELAY \| RELAY2 \| COVER \| MULTIBUTTON` |
| `power_type`        | `SH_POWER_MAINS`                          |
| `control_mode`      | `SH_CONTROL_MODE_COVER` (0x05)            |
| `config_profile`    | `SH_PROFILE_COVER_BASIC` (0x03)           |
| `reporting_mode`    | `SH_REPORTING_HYBRID`                     |
| `sensor_mask`       | `"XXXXXXXXXX"`                            |
| `input_mask`        | `"BTN3X"`                                 |

---

## MQTT-Topics (Firmware-Linie)

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-ZRL-002/meta` | ✅ | Metadaten |
| `smarthome/device/NET-ZRL-002/availability` | ✅ | Online-Status |
| `smarthome/device/NET-ZRL-002/state` | ✅ | Rollladen-Zustand (Position, Kalibrierung) |
| `smarthome/device/NET-ZRL-002/event` | ❌ | Ereignisse (Cover up/down/stop) |
| `smarthome/device/NET-ZRL-002/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-ZRL-002/command` | — | Kommandos (open, close, stop, set_position, calibrate) |
