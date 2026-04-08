# Server

Dieser Ordner enthaelt den aktuellen lokalen Serverstand fuer Phase 1.

Er ist weder nur ein nackter MQTT-Kern noch schon eine breite Serveroberflaeche.
Der reale Stand besteht aus einem kleinen technischen Kern mit engen offiziellen HTTP-Pfaden
und einer zusaetzlichen lokalen Cover-Detailseite.

## Real vorhandene Bausteine

- Mosquitto
- Node-RED
- InfluxDB als mitgestarteter Basisdienst
- SQLite als lokale Datei unter `server/sqlite/`
- aktive Flows `00`, `10`, `20`, `30`, `40`, `41`, `90`

## Reale Einstiege

- `http://localhost:1880` fuer Node-RED
- `POST /api/phase1/net-erl/relay-1` als enger offizieller Relay-Minimalpfad
- `POST /api/phase1/cover/command` als enger offizieller neutraler Cover-Pfad
- `GET /device/<device_id>` als lokale Cover-Detailseite
- `POST /api/phase1/cover/automation/<device_id>` als lokaler Save-Pfad der Detailseite

Wichtig:
- die beiden `POST /api/phase1/...`-Pfade sind der offizielle obere Minimalpfad
- die Detailseite und ihr Save-Pfad sind lokal und projektpraktisch, nicht der neutrale obere Vertrag

## Wichtige Laufzeitdateien

- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/cover_automation.json`

## Einstieg in die Doku

- `docs/public/04_server_schnellstart_phase1.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `docs/public/server/03_cover_command_und_positionssemantik.md`
