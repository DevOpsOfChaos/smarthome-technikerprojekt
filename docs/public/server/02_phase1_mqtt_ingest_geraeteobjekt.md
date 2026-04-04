# Phase 1 MQTT-Ingest und Geräteobjekt

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

Phase 1.3 fuehrt genau einen offiziellen Server-Ausgang ein:

- HTTP-Eingang: `POST /api/phase1/net-erl/relay-1`
- MQTT-Ziel: `smarthome/device/<device_id>/command`
- erlaubter Inhalt: nur `set_relay` fuer `relay_1`
- `request_id` wird serverseitig erzeugt

Diese Strecke erweitert den Ingest nicht um eine zweite Fachwelt. ACK und State laufen danach weiter ueber dieselbe bestehende Ingest-Kette zurueck.

## Topic auf Zielblock

### `meta`

- aktualisiert `identity` und `meta`
- ersetzt die bisher bekannte Metasicht
- darf unbekannte Geraete auto-anlegen
- leitet die faehigkeitsnahe Sicht aus `caps` und `device_class` ab
- ist fachlich Geraetebeschreibung plus Handshake-Sicht, nicht die Hauptwahrheit fuer Recovery
- nach einem reinen Master-Neustart ist ohne neues Node-`HELLO` keine frische `meta`-Wiederveroeffentlichung garantiert

### `availability`

- aktualisiert nur `availability`
- setzt `online`
- setzt `last_seen_at`
- veraendert keine Sensor- oder Aktorfelder

### `state`

- aktualisiert nur bekannte State-Felder
- spiegelt wenige echte Laufzeitparameter nach `config`
- setzt `last_seen_at`
- behandelt fehlende Felder nicht als Loeschsignal

### `event`

- schreibt nur in `last_event`
- ergaenzt den Zustand, ersetzt ihn nicht

### `ack`

- schreibt nur in `last_ack`
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
- SQL-Statements fuer Phase 1 stehen nur in `server/nodered/lib/sqlite_writes.js`
- der minimale Command-Bau steht nur in `server/nodered/lib/command_minimal.js`

## Unvermeidbare Node-RED-Bruecke

Node-RED-Function-Nodes koennen die lokalen Lib-Dateien nicht sinnvoll als zweiteilige Parallelwelt pflegen. Deshalb stellt der Startpfad die Module einmalig ueber `functionGlobalContext` bereit.

Diese Bruecke ist keine zweite fachliche Wahrheit, weil sie nichts ueber:

- Topic-Regeln
- Capability-Normalisierung
- Device- oder Master-Struktur
- State-, Event-, ACK- oder Status-Updates

neu definiert. Sie macht die zentrale Wahrheit nur im Flow ausfuehrbar.

## Phase-1-Grenze

Noch nicht Teil dieses Schritts:

- Timeseries-Schreiben
- weitere Verlaufs- oder Console-Historien ausser Event- und ACK-Log
- UI-Ableitungen
- jede breitere oder generische Command-Welt ausser `net_erl` Relay 1
- Wetter und Automationen

Das ist kein Mangel. Das ist Scope-Kontrolle.
