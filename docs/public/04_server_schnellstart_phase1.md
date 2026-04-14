# Server Schnellstart

## Ziel dieses Dokuments

Diese Datei ist der kleinste ehrliche Einstieg in den aktuellen Serverstand.

Sie beschreibt:
- wie der Stack real startet
- welche Oberflaechen wirklich vorhanden sind
- welche Laufzeitdateien dabei entstehen
- was bewusst noch nicht zu diesem kleinen Serverkern gehoert

## Aktueller realer Serverstand

Der aktuelle Stand besteht aus:
- Mosquitto
- Node-RED
- InfluxDB als mitgestartetem Basisdienst
- SQLite als lokaler Datei, nicht als eigenem Container
- aktiven Flows `00_boot`, `05_dashboard_runtime`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `60_dashboard_overview`, `63_dashboard_device_detail`, `90_master_diag`

Wichtige Einordnung:
- das ist ein kleiner kontrollierter Serverkern
- die sichtbare Oberflaeche ist das Dashboard mit Uebersicht und versteckter Detailseite
- es ist bewusst noch keine breite Server- oder Komfortplattform

## Was `docker-compose.yml` real macht

Der reale Startpfad liegt aktuell direkt in `server/docker-compose.yml`.

Die Compose-Startlogik des Node-RED-Dienstes macht zusaetzlich:
- Installation von `node-red-node-sqlite`, falls das Paket lokal noch fehlt
- Initialisierung des SQLite-Schemas aus `server/sqlite/00_schema_phase1.sql`
- Nachziehen kleiner Bestandsschema-Migrationen fuer Vertragsfelder
- Zusammenbau aller JSON-Dateien aus `server/nodered/flows/active/` zu `server/nodered/flows.json`
- Generierung einer temporaeren Node-RED-Settings-Datei mit dem benoetigten `functionGlobalContext`

Damit ist klar:
- der Compose-Inline-Pfad ist die reale Startwahrheit
- es gibt in diesem Repo keinen zweiten offiziellen Node-RED-Startpfad als Hauptsicht

## Reale Einstiege

### 1. Node-RED

- `http://localhost:1880`

Das ist der technische Haupteinstieg fuer den laufenden Stack.

### 2. Dashboard

- `http://localhost:1880/dashboard/`
- `http://localhost:1880/dashboard/geraet?device=<device_id>`

Das Dashboard umfasst aktuell:
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

## Kleinster Server-Contract-Smoke-Test

Aus dem Repo-Root kann der enge Vertrags-Smoke-Test gestartet werden:

```powershell
.\tests\server\server_contract_smoke.ps1
```

Wenn der Stack bereits laeuft:

```powershell
.\tests\server\server_contract_smoke.ps1 -SkipStart
```

Der Test prueft gezielt:
- numerische `caps` und deren Ableitung in die Server-Capabilities
- `button_flags`
- ACK-`source` in `device_state_latest` und `device_ack_log`
- SQL- und Migrationsfehler beim Stack-Start

Der Test braucht kein lokal installiertes `mosquitto_pub` auf dem Host.
Die MQTT-Publishes laufen ueber den Compose-Service `mosquitto`.

Ein bestandener Lauf ist ein enger Server-Vertragsnachweis.
Er ist kein vollstaendiger End-to-End-Beweis fuer alle Geraete- und UI-Pfade.

## Was nach dem Start real nachvollziehbar sein soll

- dass der Stack ueber `server/docker-compose.yml` hochkommt
- dass die aktiven Flow-Dateien zu `server/nodered/flows.json` zusammengebaut werden
- dass MQTT-Ingest, Runtime-State und SQLite-Persistenz ueber denselben kleinen Serverkern laufen
- dass das Dashboard eine Geraeteuebersicht und eine versteckte Detailseite bereitstellt

## Was dieser Stand nicht ist

Nicht erwarten:
- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienflaeche
- breite Automationen
- Komfort-Commands als oeffentliche Hauptsicht
- eine breite UI fuer alle denkbaren Ausbaustufen

## Weiterfuehrende Dateien

- `server/README.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `tests/server/phase1_ingest_checkliste.md`
