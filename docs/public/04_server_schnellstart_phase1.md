# Server Schnellstart

## Ziel dieses Dokuments

Diese Datei ist der kleinste ehrliche Einstieg in den aktuellen Serverstand.

Sie beschreibt:
- wie der Stack real startet
- welche Oberflächen wirklich vorhanden sind
- welche Laufzeitdateien dabei entstehen
- was bewusst noch nicht zu diesem kleinen Serverkern gehört

## Aktueller realer Serverstand

Der aktuelle Stand besteht aus:
- Mosquitto
- Node-RED
- SQLite als lokaler Datei, nicht als eigenem Container
- aktiven Flows `00_boot`, `05_dashboard_runtime`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `60_dashboard_overview`, `63_dashboard_device_detail`, `90_master_diag`

Wichtige Einordnung:
- das ist ein kleiner kontrollierter Serverkern
- die sichtbare Oberfläche ist das Dashboard mit Übersicht und versteckter Detailseite
- es ist bewusst noch keine breite Server- oder Komfortplattform

## Was `docker-compose.yml` real macht

Der reale Startpfad liegt aktuell direkt in `server/docker-compose.yml`.

Die Compose-Startlogik des Node-RED-Dienstes macht zusätzlich:
- Installation von `node-red-node-sqlite`, falls das Paket lokal noch fehlt
- Initialisierung des SQLite-Schemas aus `server/sqlite/00_schema_phase1.sql`
- Nachziehen kleiner Bestandsschema-Migrationen für Vertragsfelder
- Zusammenbau aller JSON-Dateien aus `server/nodered/flows/active/` zu `server/nodered/flows.json`
- Generierung einer temporären Node-RED-Settings-Datei mit dem benötigten `functionGlobalContext`

Damit ist klar:
- der Compose-Inline-Pfad ist die reale Startwahrheit
- es gibt in diesem Repo keinen zweiten offiziellen Node-RED-Startpfad als Hauptsicht

## Reale Einstiege

### 1. Node-RED

- `http://localhost:1880`

Das ist der technische Haupteinstieg für den laufenden Stack.

### 2. Dashboard

- `http://localhost:1880/dashboard/`
- `http://localhost:1880/dashboard/geraet?device=<device_id>`

Das Dashboard umfasst aktuell:
- eine zentrale Geräteübersicht
- eine versteckte Detailseite pro Gerät

Die Detailseite ist real vorhanden, aber nicht als breite Navigationsfläche gedacht.

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
2. bei Bedarf `Copy-Item .env.example .env` ausführen und lokale Werte anpassen
3. `docker compose up -d` ausführen
4. `docker compose ps` prüfen
5. `http://localhost:1880/dashboard/` öffnen

## Kleinster Server-Contract-Smoke-Test

Aus dem Repo-Root kann der enge Vertrags-Smoke-Test gestartet werden:

```powershell
.\tests\server\server_contract_smoke.ps1
```

Wenn der Stack bereits läuft:

```powershell
.\tests\server\server_contract_smoke.ps1 -SkipStart
```

Der Test prüft gezielt:
- numerische `caps` und deren Ableitung in die Server-Capabilities
- `button_flags`
- ACK-bezogene Felder im aktuellen Snapshot
- SQL- und Migrationsfehler beim Stack-Start

Der Test braucht kein lokal installiertes `mosquitto_pub` auf dem Host.
Die MQTT-Publishes laufen über den Compose-Service `mosquitto`.

Ein bestandener Lauf ist ein enger Server-Vertragsnachweis.
Er ist kein vollständiger End-to-End-Beweis für alle Geräte- und UI-Pfade.

## Was nach dem Start real nachvollziehbar sein soll

- dass der Stack über `server/docker-compose.yml` hochkommt
- dass die aktiven Flow-Dateien zu `server/nodered/flows.json` zusammengebaut werden
- dass MQTT-Ingest, Runtime-State und SQLite-Snapshot-Persistenz über denselben kleinen Serverkern laufen
- dass das Dashboard eine Geräteübersicht und eine versteckte Detailseite bereitstellt

## Was dieser Stand nicht ist

Nicht erwarten:
- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienfläche
- breite Automationen
- Komfort-Commands als öffentliche Hauptsicht
- eine breite UI für alle denkbaren Ausbaustufen

## Weiterführende Dateien

- `server/README.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `tests/server/phase1_ingest_checkliste.md`
