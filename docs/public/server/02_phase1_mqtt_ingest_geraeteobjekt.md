# MQTT-Ingest und Geräteobjekt

## Ingest-Regeln

Der Server verarbeitet MQTT nicht frei nach Bauchgefuehl, sondern nach einer festen Reihenfolge:

1. Topic erkennen
2. Geraet oder Master zuordnen
3. Payload als JSON lesen
4. passenden Block aktualisieren
5. den gemeinsamen Laufzeitzustand fortschreiben
6. die daraus zentral abgeleiteten SQLite-Writes ausfuehren

Die fachliche Wahrheit liegt dabei in den Lib-Dateien unter `server/nodered/lib/`.
Die aktiven Flows enthalten nur noch die Node-RED-Bruecken fuer:

- MQTT-Eingang und JSON-Lesen
- Aufruf der zentralen Router- und Handler-Module
- Schreiben des aktualisierten Runtime-Objekts in den globalen Node-RED-Kontext
- Ausfuehren der zentral vorbereiteten SQLite-Batches

## Minimaler Command-Ausgang

Der aktuelle Server fuehrt enge offizielle Server-Ausgaenge:

- HTTP-Eingaenge: `POST /api/phase1/net-erl/relay-1` und `POST /api/phase1/cover/command`
- MQTT-Ziel: `smarthome/device/<device_id>/command`
- erlaubter Inhalt: `set_relay` fuer `relay_1` sowie `open`, `close`, `stop`, `set_position` fuer Cover
- `request_id` wird serverseitig erzeugt
- `set_position` wird serverseitig nur weitergegeben, wenn der letzte bekannte Cover-State `cover_calibrated` nicht `false` ist

Diese Strecke erweitert den Ingest nicht um eine zweite Fachwelt. ACK und State laufen danach weiter ueber dieselbe bestehende Ingest-Kette zurueck.

Auf `main` ist der enge Relay-Minimalpfad fuer den realen Node `net_erl_01` belegt:

- echter HTTP-Aufruf gegen `POST /api/phase1/net-erl/relay-1`
- MQTT-Command ueber `smarthome/device/net_erl_01/command`
- ACK und State ueber denselben realen Master-/Geraetepfad zurueck
- erneute Nachweise nach Server-Restart, Node-Recovery und Master-Recovery
- Hardware-Rerun fuer den frueheren Master-Recovery-Fehler mit bestaetigt erhaltenen Voll-States

Fuer Cover gibt es jetzt denselben engen serverseitigen Ausgang auf demselben MQTT-Command-Topic, ohne daraus eine allgemeine Command-Welt zu machen.

## Topic auf Zielblock

### `meta`

- aktualisiert `identity` und `meta`
- ersetzt die bisher bekannte Metasicht
- darf unbekannte Geraete auto-anlegen
- leitet die faehigkeitsnahe Sicht aus numerischer `caps`-Bitmaske oder aus `caps`-Listen sowie `device_class` ab
- ist fachlich Geraetebeschreibung plus Handshake-Sicht, nicht die Hauptwahrheit fuer Recovery
- nach einem reinen Master-Neustart ist ohne neues Node-`HELLO` keine frische `meta`-Wiederveroeffentlichung garantiert

### `availability`

- aktualisiert nur `availability`
- setzt `online`
- setzt `last_seen_at`
- veraendert keine Sensor- oder Aktorfelder

### `state`

- aktualisiert nur bekannte State-Felder
- traegt fuer `cover` `cover_mode`, `cover_state`, optional `cover_position` und `cover_calibrated`
- traegt fuer batteriebetriebene Sensoren `battery_pct`, `battery_mv`, `window_open`, `rain_raw` und `button_flags`
- spiegelt wenige echte Laufzeitparameter nach `config`
- setzt `last_seen_at`
- behandelt fehlende Felder nicht als Loeschsignal

### `event`

- schreibt nur in `last_event`
- mappt das vom Master gelieferte `event` auf `event_label` und `trigger` auf `event_trigger`
- ergaenzt den Zustand, ersetzt ihn nicht

### `ack`

- schreibt nur in `last_ack`
- haelt neben Request, Channel, Status, Status-Code, ACK-Typ und Sequenz auch die vom Master gelieferte `source`
- ist fuer Nachvollziehbarkeit da, nicht fuer den fachlichen Hauptzustand

### `master/status` und `master/event`

- laufen komplett getrennt vom normalen Geraetepfad
- dienen nur der Server- und Verbindungsdiagnose

## Zielstruktur pro Geraet

```text
identity
meta
availability
state
config
last_event
last_ack
diagnostics
```

Diese Struktur ist absichtlich simpel. Wer hier wieder Sondermodelle pro Geraetetyp einzieht, produziert denselben Wartungsschaden wie vorher, nur mit anderem Dateinamen.

## Bekannte Pflichtregeln

- `state` ist die Hauptwahrheit fuer den sichtbaren Geraetezustand
- `event` darf den State nicht heimlich ersetzen
- `ack` darf keine Bedienerfolge vortaeuschen
- ein Master-Recovery ist fachlich ueber frischen Master-Status plus frische `availability`/`state` zu bewerten, nicht ueber erzwungen frische `meta`
- SQLite bekommt ihre Writes aus derselben Handlerkette wie der Runtime-State
- `meta`, `availability` und `state` duerfen unbekannte Geraete robust anlegen
- der Master bleibt separat
- Topic-Erkennung steht nur in `server/nodered/lib/topic_router.js`
- Block-Updates stehen nur in `server/nodered/lib/device_store.js`
- die Zuordnung Topic -> Handler steht nur in `server/nodered/lib/topic_handlers.js`
- SQL-Statements fuer den aktuellen Server stehen nur in `server/nodered/lib/sqlite_writes.js`
- der minimale Command-Bau steht nur in `server/nodered/lib/command_minimal.js`

## Unvermeidbare Node-RED-Bruecke

Node-RED-Function-Nodes koennen die lokalen Lib-Dateien nicht sinnvoll als zweiteilige Parallelwelt pflegen. Deshalb stellt der Startpfad die Module einmalig ueber `functionGlobalContext` bereit.

Diese Bruecke ist keine zweite fachliche Wahrheit, weil sie nichts ueber:

- Topic-Regeln
- Capability-Normalisierung
- Device- oder Master-Struktur
- State-, Event-, ACK- oder Status-Updates

neu definiert. Sie macht die zentrale Wahrheit nur im Flow ausfuehrbar.

## Recovery-Status des bestaetigten Kerns

Fuer den aktuell oeffentlich bestaetigten Minimalpfad ist belegt:

- Node-Recovery liefert wieder frische `meta`, `availability` und `state`
- reiner Master-Recovery ist fachlich nicht an frische `meta` gebunden
- der fruehere Null-/Minimal-State nach Master-Recovery war ein echter Master-Projektionsfehler und kein Server-Persistenzfehler
- der Master-Fix ist per Hardware-Rerun auf `MASTER-001` fuer den realen Pfad bestaetigt

## Grenze

Noch nicht Teil dieses Schritts:

- Timeseries-Schreiben
- weitere Verlaufs- oder Console-Historien ausser Event- und ACK-Log
- UI-Ableitungen
- jede breitere oder generische Command-Welt ueber die engen Relay- und Cover-Pfade hinaus
- serverseitige Rolladen-Komfortlogik
- Wetter und Automationen

Das ist kein Mangel. Das ist Scope-Kontrolle.
