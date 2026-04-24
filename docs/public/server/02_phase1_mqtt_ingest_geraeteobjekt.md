# MQTT-Ingest und Geräteobjekt

## Ingest-Regeln

Der Server verarbeitet MQTT nicht frei nach Bauchgefühl, sondern nach einer festen Reihenfolge:

1. Topic erkennen
2. Gerät oder Master zuordnen
3. Payload als JSON lesen
4. passenden Block aktualisieren
5. den gemeinsamen Laufzeitzustand fortschreiben
6. die daraus zentral abgeleiteten SQLite-Writes ausführen

Die fachliche Wahrheit liegt dabei in den Lib-Dateien unter `server/nodered/lib/`.
Die aktiven Flows enthalten nur noch die Node-RED-Brücken für:

- MQTT-Eingang und JSON-Lesen
- Aufruf der zentralen Router- und Handler-Module
- Schreiben des aktualisierten Runtime-Objekts in den globalen Node-RED-Kontext
- Ausführen der zentral vorbereiteten SQLite-Batches

## Minimaler Command-Ausgang

Der aktuelle Server führt enge offizielle Server-Ausgänge:

- HTTP-Eingänge: `POST /api/phase1/net-erl/relay-1` und `POST /api/phase1/cover/command`
- MQTT-Ziel: `smarthome/device/<device_id>/command`
- erlaubter Inhalt: `set_relay` für `relay_1` sowie `open`, `close`, `stop`, `set_position` für Cover
- `request_id` wird serverseitig erzeugt
- `set_position` wird serverseitig nur weitergegeben, wenn der letzte bekannte Cover-State `cover_calibrated` nicht `false` ist

Diese Strecke erweitert den Ingest nicht um eine zweite Fachwelt. ACK und State laufen danach weiter über dieselbe bestehende Ingest-Kette zurück.

Auf `main` ist der enge Relay-Minimalpfad für den realen Node `net_erl_01` belegt:

- echter HTTP-Aufruf gegen `POST /api/phase1/net-erl/relay-1`
- MQTT-Command über `smarthome/device/net_erl_01/command`
- ACK und State über denselben realen Master-/Gerätepfad zurück
- erneute Nachweise nach Server-Restart, Node-Recovery und Master-Recovery
- Hardware-Rerun für den früheren Master-Recovery-Fehler mit bestätigt erhaltenen Voll-States

Für Cover gibt es jetzt denselben engen serverseitigen Ausgang auf demselben MQTT-Command-Topic, ohne daraus eine allgemeine Command-Welt zu machen.

## Topic auf Zielblock

### `meta`

- aktualisiert `identity` und `meta`
- ersetzt die bisher bekannte Metasicht
- darf unbekannte Geräte auto-anlegen
- leitet die fähigkeitsnahe Sicht aus numerischer `caps`-Bitmaske oder aus `caps`-Listen sowie `device_class` ab
- ist fachlich Gerätebeschreibung plus Handshake-Sicht, nicht die Hauptwahrheit für Recovery
- nach einem reinen Master-Neustart ist ohne neues Node-`HELLO` keine frische `meta`-Wiederveröffentlichung garantiert

### `availability`

- aktualisiert nur `availability`
- setzt `online`
- setzt `last_seen_at`
- verändert keine Sensor- oder Aktorfelder

### `state`

- aktualisiert nur bekannte State-Felder
- trägt für `cover` `cover_mode`, `cover_state`, optional `cover_position` und `cover_calibrated`
- trägt für batteriebetriebene Sensoren `battery_pct`, `battery_mv`, `window_open`, `rain_raw` und `button_flags`
- spiegelt wenige echte Laufzeitparameter nach `config`
- setzt `last_seen_at`
- behandelt fehlende Felder nicht als Löschsignal

### `event`

- schreibt nur in `last_event`
- mappt das vom Master gelieferte `event` auf `event_label` und `trigger` auf `event_trigger`
- ergänzt den Zustand, ersetzt ihn nicht
- bleibt Teil des aktuellen Geräte-Snapshots, aber nicht Teil einer separaten Verlaufsspeicherung

### `ack`

- schreibt nur in `last_ack`
- hält neben Request, Channel, Status, Status-Code, ACK-Typ und Sequenz auch die vom Master gelieferte `source`
- ist für Nachvollziehbarkeit da, nicht für den fachlichen Hauptzustand
- bleibt Teil des aktuellen Geräte-Snapshots, aber nicht Teil einer separaten ACK-Historie

### `master/status`

- läuft komplett getrennt vom normalen Gerätepfad
- dient der Server- und Verbindungsdiagnose

## Zielstruktur pro Gerät

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

Diese Struktur ist absichtlich simpel. Wer hier wieder Sondermodelle pro Gerätetyp einzieht, produziert denselben Wartungsschaden wie vorher, nur mit anderem Dateinamen.

## Bekannte Pflichtregeln

- `state` ist die Hauptwahrheit für den sichtbaren Gerätezustand
- `event` darf den State nicht heimlich ersetzen
- `ack` darf keine Bedienerfolge vortäuschen
- ein Master-Recovery ist fachlich über frischen Master-Status plus frische `availability`/`state` zu bewerten, nicht über erzwungen frische `meta`
- SQLite bekommt ihre Writes aus derselben Handlerkette wie der Runtime-State
- `meta`, `availability` und `state` dürfen unbekannte Geräte robust anlegen
- der Master bleibt separat
- Topic-Erkennung steht nur in `server/nodered/lib/topic_router.js`
- Block-Updates stehen nur in `server/nodered/lib/device_store.js`
- die Zuordnung Topic -> Handler steht nur in `server/nodered/lib/topic_handlers.js`
- SQL-Statements für den aktuellen Server stehen nur in `server/nodered/lib/sqlite_writes.js`
- der minimale Command-Bau steht nur in `server/nodered/lib/command_minimal.js`

## Unvermeidbare Node-RED-Brücke

Node-RED-Function-Nodes können die lokalen Lib-Dateien nicht sinnvoll als zweiteilige Parallelwelt pflegen. Deshalb stellt der Startpfad die Module einmalig über `functionGlobalContext` bereit.

Diese Brücke ist keine zweite fachliche Wahrheit, weil sie nichts über:

- Topic-Regeln
- Capability-Normalisierung
- Device- oder Master-Struktur
- State-, Event-, ACK- oder Status-Updates

neu definiert. Sie macht die zentrale Wahrheit nur im Flow ausführbar.

## Recovery-Status des bestätigten Kerns

Für den aktuell öffentlich bestätigten Minimalpfad ist belegt:

- Node-Recovery liefert wieder frische `meta`, `availability` und `state`
- reiner Master-Recovery ist fachlich nicht an frische `meta` gebunden
- der frühere Null-/Minimal-State nach Master-Recovery war ein echter Master-Projektionsfehler und kein Server-Persistenzfehler
- der Master-Fix ist per Hardware-Rerun auf `MASTER-001` für den realen Pfad bestätigt

## Grenze

Noch nicht Teil dieses Schritts:

- Zeitreihen-Schreiben
- weitere Verlaufs- oder Console-Historien
- UI-Ableitungen über den aktuellen Dashboard-Kern hinaus
- jede breitere oder generische Command-Welt über die engen Relay- und Cover-Pfade hinaus
- serverseitige Rolladen-Komfortlogik
- Wetter und Automationen

Das ist kein Mangel. Das ist Scope-Kontrolle.
