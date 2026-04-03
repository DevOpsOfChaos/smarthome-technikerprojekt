# Phase 1 Dateirollen

## Allowlist-Dateien und ihre Rolle

### Server-Basis

- `server/.env.example`: minimale lokale Beispielwerte fuer Ports, Influx-Setup und SQLite-Pfad
- `server/docker-compose.yml`: startet nur die benoetigten Basisdienste und baut die aktiven Flow-Dateien zu `flows.json` zusammen

### Aktive Flows

- `server/nodered/flows/active/00_boot.json`: legt den Runtime-State an
- `server/nodered/flows/active/10_mqtt_ingest.json`: MQTT-Ingest, JSON-Lesen, Topic-Routing
- `server/nodered/flows/active/20_device_store.json`: pflegt das gemeinsame Geraeteobjekt
- `server/nodered/flows/active/90_master_diag.json`: trennt Masterstatus und Masterevents vom Geraetepfad

### Kleine Hilfsmodule

- `server/nodered/lib/topic_router.js`: erkennt unterstuetzte Topics und ihre Zielbloecke
- `server/nodered/lib/device_store.js`: enthaelt die fachliche Zielstruktur und blockweise Update-Helfer
- `server/nodered/lib/topic_handlers.js`: exportiert die klar benannten Handlerfunktionen fuer Meta, Availability, State, Event, ACK und Master
- `server/nodered/lib/capability_helpers.js`: normalisiert und leitet Faehigkeiten ab
- `server/nodered/lib/time_helpers.js`: kleine Helfer fuer Zeit- und Typnormalisierung

### Persistenz- und Nachweisbasis

- `server/sqlite/00_schema_phase1.sql`: minimales Schema fuer `devices`, `device_state_latest`, Event-/ACK-Logs und Masterdiagnose
- `tests/server/phase1_ingest_checkliste.md`: manueller Nachweis fuer Auto-Anlage, partielle State-Updates und getrennten Masterpfad

### Oeffentliche Doku

- `docs/public/server/01_server_v1_ueberblick.md`: fachlicher Zuschnitt von Phase 1
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`: Mapping-Regeln fuer MQTT-Ingest und Geraeteobjekt
- `docs/public/server/03_phase1_dateirollen.md`: Rollen der umgesetzten Dateien

## Bewusst noch nicht umgesetzt

- Timeseries-Writes
- UI-Ableitungen
- Command-Pfad
- Logs- und MQTT-Console-Datenhaltung

Wenn diese Themen jetzt hineingezogen wuerden, waere das kein Fortschritt, sondern wieder Scope-Flucht.
