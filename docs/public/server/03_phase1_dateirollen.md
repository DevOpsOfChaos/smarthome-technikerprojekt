# Phase 1 Dateirollen

## Allowlist-Dateien und ihre Rolle

### Server-Basis

- `server/.env.example`: minimale lokale Beispielwerte fuer Ports, Influx-Setup und SQLite-Pfad
- `server/docker-compose.yml`: startet nur die benoetigten Basisdienste, baut die aktiven Flow-Dateien zu `flows.json` zusammen und stellt die Lib-Module als globale Node-RED-Bruecke bereit

### Aktive Flows

- `server/nodered/flows/active/00_boot.json`: initialisiert den Runtime-State ueber die zentrale Lib-Schicht
- `server/nodered/flows/active/10_mqtt_ingest.json`: MQTT-Ingest, JSON-Lesen, Topic-Routing ueber `topic_router.js`
- `server/nodered/flows/active/20_device_store.json`: duenne Node-RED-Bruecken fuer Device-Handler aus `topic_handlers.js`
- `server/nodered/flows/active/30_sqlite_persist.json`: fuehrt nur die zentral vorbereiteten SQLite-Batches gegen die Phase-1-Datenbank aus
- `server/nodered/flows/active/40_command_minimal.json`: offizieller HTTP-Command-Eingang fuer genau einen Fall: `net_erl` Relay 1
- `server/nodered/flows/active/90_master_diag.json`: duenne Node-RED-Bruecken fuer Master-Handler aus `topic_handlers.js`

### Kleine Hilfsmodule

- `server/nodered/lib/topic_router.js`: erkennt unterstuetzte Topics und ihre Zielbloecke
- `server/nodered/lib/device_store.js`: enthaelt die fachliche Zielstruktur und blockweise Update-Helfer
- `server/nodered/lib/topic_handlers.js`: exportiert die klar benannten Handlerfunktionen und die zentrale Zuordnung von Routing-Kontext zu Handlern
- `server/nodered/lib/sqlite_writes.js`: leitet aus dem bereits aktualisierten Runtime-/Masterobjekt die einzigen Phase-1.2-SQLite-Writes ab
- `server/nodered/lib/command_minimal.js`: validiert den engen `net_erl`-Relay-1-Command, erzeugt die `request_id` und baut den MQTT-Payload
- `server/nodered/lib/capability_helpers.js`: normalisiert und leitet Faehigkeiten ab
- `server/nodered/lib/time_helpers.js`: kleine Helfer fuer Zeit- und Typnormalisierung

### Persistenz- und Nachweisbasis

- `server/sqlite/00_schema_phase1.sql`: minimales Schema fuer `devices`, `device_state_latest`, Event-/ACK-Logs und Masterdiagnose
- `server/nodered/package.json`: pinnt den benoetigten `node-red-node-sqlite`-Baustein fuer die lokale Phase-1.2-Ausfuehrung
- `server/docker-compose.yml`: initialisiert das SQLite-Schema vor dem Start und haengt die aktiven Flows mit dem generierten Node-RED-Settings-Kontext zusammen
- `tests/server/phase1_ingest_checkliste.md`: manueller Nachweis fuer Auto-Anlage, partielle State-Updates und getrennten Masterpfad

### Oeffentliche Doku

- `docs/public/server/01_server_v1_ueberblick.md`: fachlicher Zuschnitt von Phase 1
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`: Mapping-Regeln fuer MQTT-Ingest und Geraeteobjekt
- `docs/public/server/03_phase1_dateirollen.md`: Rollen der umgesetzten Dateien

## Bewusst noch nicht umgesetzt

- Timeseries-Writes
- UI-Ableitungen
- jeder breitere Command-Pfad ausser `net_erl` Relay 1
- weitere Logs- und MQTT-Console-Datenhaltung ausser den Phase-1.2-Append-Logs

## Architekturregel fuer Phase 1.2

Die Flows sind absichtlich nicht mehr die fachliche Hauptquelle.
Wenn wieder Coercion, Capability-Aliase, Device-Erzeugung, Topic-Mapping oder SQL-Fachregeln inline in Function-Nodes auftauchen, ist die Phase-1.1/1.2-Linie wieder gebrochen.

Wenn diese Themen jetzt hineingezogen wuerden, waere das kein Fortschritt, sondern wieder Scope-Flucht.
