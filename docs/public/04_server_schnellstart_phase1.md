# Server Schnellstart Phase 1

## Ziel dieses Dokuments

Diese Datei ist der kleinste ehrliche Einstieg in den aktuellen Serverstand.

Sie beschreibt nicht nur, wie der Stack startet, sondern auch:
- was real laeuft
- welche URLs und HTTP-Pfade wirklich vorhanden sind
- was offizieller neutraler Minimalpfad ist
- was bereits lokale Bedienflaeche ist
- welche Laufzeitdateien dabei entstehen

## Aktueller realer Serverstand

Der aktuelle Stand besteht aus:
- Mosquitto
- Node-RED
- InfluxDB als mitgestartetem Basisdienst
- SQLite als lokaler Datei, nicht als eigenem Container
- aktiven Flows `00_boot`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `40_command_minimal`, `41_cover_automation_detail`, `90_master_diag`

Wichtige Einordnung:
- das ist nicht nur ein nackter Ingest-Kern
- es ist aber auch noch keine breite allgemeine Serveroberflaeche
- real vorhanden ist ein kleiner, aufrufbarer Serverstand mit engem neutralem Command-Pfad und einer lokalen Cover-Detailseite

## Was `docker-compose.yml` real macht

Beim Start werden nicht nur Container hochgezogen.

Die Compose-Startlogik des Node-RED-Dienstes macht zusaetzlich:
- Installation von `node-red-node-sqlite`, falls das Paket lokal noch fehlt
- Initialisierung des SQLite-Schemas aus `server/sqlite/00_schema_phase1.sql`
- Nachziehen kleiner Phase-1-Migrationen fuer `cover_direction` und `cover_calibrated`
- Zusammenbau aller JSON-Dateien aus `server/nodered/flows/active/` zu `server/nodered/flows.json`
- Generierung einer temporaeren Node-RED-Settings-Datei mit `functionGlobalContext` fuer:
  - `topicRouter`
  - `deviceStore`
  - `topicHandlers`
  - `capabilityHelpers`
  - `timeHelpers`
  - `commandMinimal`
  - `coverAutomation`

Damit ist der reale Startzustand enger und konkreter als ein bloes `docker compose up`.

## Reale Einstiege und ihre Einordnung

### 1. Technischer Grundeinstieg

- `http://localhost:1880`

Das ist der sichtbare Node-RED-Haupteinstieg.
Hier startet der Stack real.

### 2. Offizielle neutrale HTTP-Minimalpfade

- `POST /api/phase1/net-erl/relay-1`
- `POST /api/phase1/cover/command`

Diese beiden Pfade sind der offizielle obere Minimalpfad.
Sie bauen serverseitig `request_id`, publizieren auf `smarthome/device/<device_id>/command`
und erwarten ACK/State ueber dieselbe Ingest-Kette zurueck.

Fuer Cover gelten dabei real:
- erlaubte Commands: `open`, `close`, `stop`, `set_position`
- `set_position` nur bei bekanntem kalibrierten Cover-Zustand
- bei `cover_calibrated=false` antwortet der Server mit `409 not_calibrated`

### 3. Lokale Bedienflaeche

- `GET /device/<device_id>`
- `POST /api/phase1/cover/automation/<device_id>`

Diese Pfade existieren real und sind aufrufbar.
Sie gehoeren aber nicht zum neutralen oberen Vertrag.

Sie bilden eine kleine lokale Cover-Detailseite mit:
- Statusanzeige fuer ein bekanntes Cover-Geraet
- genau zwei Zeit/Wert-Slots pro Tag
- festen Zielwerten `0`, `25`, `50`, `75`, `100`
- einem Minutentick, der intern denselben offiziellen Cover-Command-Baustein nutzt

Wichtige Grenze:
- die Detailseite funktioniert nur fuer Cover-Geraete, die bereits im Runtime-State bekannt sind
- unbekannte oder nicht passende Geraete liefern hier bewusst `404`
- das ist keine allgemeine Geraeteoberflaeche und keine breite Automationsplattform

## Wichtige Laufzeitdateien

Im aktuellen lokalen Stand entstehen oder werden aktiv benutzt:

- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/cover_automation.json`
- `server/nodered/node_modules/`

Davon sind insbesondere lokal und nicht Teil des Repo-Inhalts:
- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/cover_automation.json`
- `server/nodered/node_modules/`

## Startablauf

1. in den Ordner `server/` wechseln
2. bei Bedarf `server/.env.example` nach `.env` kopieren und lokale Werte anpassen
3. `docker compose up -d` ausfuehren
4. `http://localhost:1880` oeffnen
5. fuer den neutralen Kern `tests/server/phase1_ingest_checkliste.md` verwenden
6. fuer die lokale Cover-Detailseite sicherstellen, dass das Cover-Geraet bereits via `meta`, `availability` und `state` im Runtime-State sichtbar ist

## Was nach dem Start real nachvollziehbar sein soll

- welche MQTT-Themen Phase 1 wirklich verarbeitet
- dass Geraete- und Masterpfad getrennt bleiben
- dass der gemeinsame Runtime-State und die SQLite-Writes aus derselben Handlerkette kommen
- dass die beiden neutralen HTTP-Minimalpfade real MQTT-Commands erzeugen
- dass es zusaetzlich eine kleine lokale Cover-Detailseite gibt

## Was dieser Stand nicht ist

Nicht erwarten:
- breite allgemeine Geraeteoberflaeche
- allgemeine UI fuer alle Geraeteklassen
- allgemeine Automationsplattform
- Wetter, Regeln oder Komfortwelten ueber den engen Cover-Lokalpfad hinaus
- eine breite Command-Matrix ueber die beiden offiziellen Minimalpfade hinaus

## Weiterfuehrende Dateien

- `server/README.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `docs/public/server/03_cover_command_und_positionssemantik.md`
- `docs/public/server/04_master_dynamische_registry_und_cover_pfad.md`
- `tests/server/phase1_ingest_checkliste.md`
- `PROTOKOLL_2026-04-08_server_inventur_und_produktflaeche.txt`
