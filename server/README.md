# Server V1

Bereichs-README fuer `server/`.
Dieser Stand bleibt bewusst klein und kontrolliert.

Nicht diese Datei:

- Gesamtarchitektur und Entscheidungen: `../docs/README.md`, `../docs/DECISIONS.md`
- belegter Gesamtstatus: `../docs/14_test_und_nachweisstand.md`

## Rolle der Serverbasis

V1 liefert die kleine serverseitige Grundlage plus einen engen Dashboard-Schnitt:

- Mosquitto
- Node-RED mit kleinem FlowFuse-Dashboard
- SQLite
- InfluxDB als bereits vorhandener Basisdienst

## Scope

- MQTT-Ingest fuer die engen V1-Themen
- Topic-Routing in Device- und Masterpfad
- gemeinsames Laufzeitobjekt pro Geraet
- separater Masterzustand
- minimale SQLite-Persistenz fuer `devices`, `device_state_latest` und `master_status`
- eine zentrale Geraeteuebersicht
- eine versteckte Detailseite pro Geraet

## Bewusst nicht Teil dieser Stufe

- Diagramme
- Wetter
- volle Logs- oder MQTT-Konsole
- Commands als Komfortpfad
- Automationen
- SIM-spezifischer Ausbau
- grosse Konfigurationswelt

## Bereichsdoku

- `../docs/public/server/01_server_v1_ueberblick.md`
- `../docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `../docs/public/server/03_phase1_dateirollen.md`
- `flows/README.md`
- `db/README.md`
- `nodered/README.md`

## Start

Im Ordner `server/`:

1. optional `Copy-Item .env.example .env`
2. lokale Werte in `.env` setzen
3. `docker compose up --build -d`
4. `docker compose ps`

Der echte V1-Startpfad liegt aktuell direkt in `docker-compose.yml`:
- Node-RED startet aus dem offiziellen `nodered/node-red:3.1`-Image.
- SQLite-Schema, kleine V1-Migrationen, `flows.json` und die generierten Settings werden beim Containerstart inline vorbereitet.
- `nodered/settings.js` bleibt die Basisdatei, die Laufzeit erweitert sie beim Start um den V1-`functionGlobalContext`.

## Offizielle V1-Dateien

- `docker-compose.yml`
- `.env.example`
- `sqlite/00_schema_phase1.sql`
- `nodered/settings.js`
- `nodered/flows/active/*.json`
- `nodered/lib/*.js`

## Lokal, nicht versioniert

- `.env`
- `config/.env`
- `config/mosquitto/config/` inklusive lokaler Auth-Dateien
- lokale Compose-Overrides

## Ehrliche Grenze

Wenn spaeter Diagramme, Wetter, Logs oder Commands dazukommen sollen, muessen sie auf diesem kleinen Ingest- und Dashboard-Kern aufbauen.
Der aktuelle Stand ist absichtlich nicht die Komfortebene.
