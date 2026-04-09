# Server Schnellstart Phase 1

## Ziel dieses Dokuments

Diese Datei ist der kleinste ehrliche Einstieg in den aktuellen Server-V1-Stand.

Sie beschreibt:
- wie der Stack real startet
- welche Oberflaechen in dieser Stufe wirklich vorhanden sind
- welche Laufzeitdateien dabei entstehen
- was bewusst noch nicht zu diesem kleinen V1-Kern gehoert

## Aktueller realer Serverstand

Der aktuelle Stand besteht aus:
- Mosquitto
- Node-RED
- InfluxDB als mitgestartetem Basisdienst
- SQLite als lokaler Datei, nicht als eigenem Container
- aktiven Flows `00_boot`, `05_dashboard_runtime`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `60_dashboard_overview`, `63_dashboard_device_detail`, `90_master_diag`

Wichtige Einordnung:
- das ist ein kleiner kontrollierter Serverkern
- die sichtbare V1-Oberflaeche ist das Dashboard mit Uebersicht und versteckter Detailseite
- es ist bewusst noch keine breite Server- oder Komfortplattform

## Was `docker-compose.yml` real macht

Der reale V1-Startpfad liegt aktuell direkt in `server/docker-compose.yml`.

Die Compose-Startlogik des Node-RED-Dienstes macht zusaetzlich:
- Installation von `node-red-node-sqlite`, falls das Paket lokal noch fehlt
- Initialisierung des SQLite-Schemas aus `server/sqlite/00_schema_phase1.sql`
- Nachziehen kleiner Phase-1-Migrationen fuer `cover_direction` und `cover_calibrated`
- Zusammenbau aller JSON-Dateien aus `server/nodered/flows/active/` zu `server/nodered/flows.json`
- Generierung einer temporaeren Node-RED-Settings-Datei mit dem benoetigten V1-`functionGlobalContext`

Damit ist klar:
- der Compose-Inline-Pfad ist die reale Startwahrheit
- es gibt in diesem Repo keinen zweiten offiziellen Node-RED-Startpfad als Hauptsicht

## Reale Einstiege

### 1. Node-RED

- `http://localhost:1880`

Das ist der technische Haupteinstieg fuer den laufenden Stack.

### 2. Dashboard V1

- `http://localhost:1880/dashboard/`
- `http://localhost:1880/dashboard/geraet?device=<device_id>`

Das Dashboard V1 umfasst aktuell:
- eine zentrale Geraeteuebersicht
- eine versteckte Detailseite pro Geraet

Die Detailseite ist real vorhanden, aber nicht als breite Navigationsflaeche gedacht.

## Wichtige Laufzeitdateien

Im aktuellen lokalen Stand entstehen oder werden aktiv benutzt:
- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/node_modules/`

Davon sind lokal und nicht Teil des Repo-Inhalts:
- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/node_modules/`

## Startablauf

1. in den Ordner `server/` wechseln
2. bei Bedarf `Copy-Item .env.example .env` ausfuehren und lokale Werte anpassen
3. `docker compose up -d` ausfuehren
4. `docker compose ps` pruefen
5. `http://localhost:1880/dashboard/` oeffnen

## Was nach dem Start real nachvollziehbar sein soll

- dass der Stack ueber `server/docker-compose.yml` hochkommt
- dass die aktiven Flow-Dateien zu `server/nodered/flows.json` zusammengebaut werden
- dass MQTT-Ingest, Runtime-State und SQLite-Persistenz ueber denselben kleinen V1-Kern laufen
- dass das Dashboard V1 eine Geraeteuebersicht und eine versteckte Detailseite bereitstellt

## Was dieser Stand nicht ist

Nicht erwarten:
- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienflaeche
- breite Automationen
- Komfort-Commands als oeffentliche V1-Hauptsicht
- eine breite UI fuer alle denkbaren Ausbaustufen

## Weiterfuehrende Dateien

- `server/README.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `tests/server/phase1_ingest_checkliste.md`
