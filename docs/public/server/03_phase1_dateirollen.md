# Phase 1 Dateirollen

## Allowlist-Dateien und ihre Rolle

### Server-Basis

- `server/.env.example`: minimale lokale Beispielwerte fuer Ports, Influx-Setup und SQLite-Pfad
- `server/docker-compose.yml`: startet die benoetigten Basisdienste, initialisiert SQLite, zieht kleine V1-Migrationen nach, baut die aktiven Flow-Dateien zu `flows.json` zusammen und erzeugt die erweiterte Node-RED-Laufzeitkonfiguration inline
- `server/nodered/settings.js`: Basisdatei fuer Node-RED; die erweiterte Settings-Datei entsteht erst beim Start

### Aktive Kernflows

- `server/nodered/flows/active/00_boot.json`: initialisiert den Runtime-State ueber die zentrale Lib-Schicht
- `server/nodered/flows/active/05_dashboard_runtime.json`: definiert Dashboard-Basis, Seiten und Gruppen fuer den V1-Schnitt
- `server/nodered/flows/active/10_mqtt_ingest.json`: MQTT-Ingest, JSON-Lesen und Topic-Routing ueber `topic_router.js`
- `server/nodered/flows/active/20_device_store.json`: duenne Node-RED-Bruecken fuer Device-Handler aus `topic_handlers.js`
- `server/nodered/flows/active/30_sqlite_persist.json`: fuehrt die vorbereiteten SQLite-Writes gegen die Phase-1-Datenbank aus
- `server/nodered/flows/active/60_dashboard_overview.json`: liefert die zentrale Geraeteuebersicht
- `server/nodered/flows/active/63_dashboard_device_detail.json`: liefert die versteckte Detailseite pro Geraet
- `server/nodered/flows/active/90_master_diag.json`: duenne Node-RED-Bruecken fuer Master-Handler aus `topic_handlers.js`

### Kleine Hilfsmodule

- `server/nodered/lib/topic_router.js`: erkennt unterstuetzte Topics und ihre Zielbloecke
- `server/nodered/lib/topic_handlers.js`: exportiert die klar benannten Handlerfunktionen und die zentrale Zuordnung von Routing-Kontext zu Handlern
- `server/nodered/lib/device_store.js`: enthaelt die fachliche Zielstruktur und die schemaengen Update-Helfer fuer Runtime und Persistenz
- `server/nodered/lib/sqlite_writes.js`: leitet aus dem aktualisierten Runtime- und Masterobjekt die SQLite-Writes ab
- `server/nodered/lib/dashboard_v1.js`: baut die SQL-Abfragen und Payloads fuer Uebersicht und Detailseite
- `server/nodered/lib/capability_helpers.js`: normalisiert und leitet Faehigkeiten ab
- `server/nodered/lib/time_helpers.js`: kleine Helfer fuer Zeit- und Typnormalisierung

### Vorhandene Zusatzpfade ausserhalb des oeffentlichen V1-Schwerpunkts

Diese Dateien liegen im Repo, sind aber nicht die Hauptsicht des aktuellen oeffentlichen V1-Kerns:

- `server/nodered/flows/active/40_command_minimal.json`
- `server/nodered/flows/active/41_cover_automation_detail.json`
- `server/nodered/lib/command_minimal.js`
- `server/nodered/lib/cover_automation.js`

Sie aendern nicht die aktuelle oeffentliche Hauptaussage:
- Compose-Inline-Startpfad
- enger Ingest-/Store-/Persistenzkern
- Dashboard V1 mit Uebersicht und Detailseite

### Persistenz- und Nachweisbasis

- `server/sqlite/00_schema_phase1.sql`: minimales Schema fuer `devices`, `device_state_latest` und `master_status`
- `server/nodered/package.json`: pinnt den benoetigten `node-red-node-sqlite`-Baustein
- `server/sqlite/smarthome_phase1.db`: lokale Laufzeitdatenbank
- `server/nodered/flows.json`: lokal generierter Zusammenbau der aktiven Flow-Dateien
- `tests/server/phase1_ingest_checkliste.md`: manueller Nachweis fuer Auto-Anlage, partielle State-Updates und getrennten Masterpfad

### Oeffentliche Doku

- `docs/public/04_server_schnellstart_phase1.md`: kleinster ehrlicher Einstieg in den aktuellen Server-V1-Stand
- `docs/public/server/01_server_v1_ueberblick.md`: fachlicher Zuschnitt von Phase 1
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`: Mapping-Regeln fuer MQTT-Ingest und Geraeteobjekt
- `docs/public/server/03_phase1_dateirollen.md`: Rollen der umgesetzten Dateien
- `docs/public/server/04_master_dynamische_registry_und_cover_pfad.md`: Ziel- und Folgestufe, nicht die aktuelle V1-Hauptsicht

## Bewusst noch nicht Teil dieser Stufe

- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienflaeche
- breite Automationen
- breite Komfort-Commands als oeffentliche V1-Hauptsicht
- eine breite UI ueber Uebersicht und Detailseite hinaus

## Architekturregel fuer Phase 1

Die oeffentliche Doku folgt dem realen V1-Pfad:

- Startwahrheit liegt in `server/docker-compose.yml`
- Dashboard V1 bedeutet aktuell Uebersicht plus versteckte Detailseite
- alte oder zusaetzliche Hilfspfade duerfen nicht als Hauptplattform beschrieben werden

Wenn die Doku wieder eine zweite Start- oder Produktsicht aufmacht, ist sie nicht ehrlicher, sondern unpraeziser.
