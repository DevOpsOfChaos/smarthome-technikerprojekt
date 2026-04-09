# Server V1 Überblick

## Ziel von Phase 1

Phase 1 zieht nur den kleinen kontrollierten Serverkern hoch:

- MQTT-Ingest fuer Geraete und Master
- Topic-Routing auf kleine Handler
- gemeinsames Geraeteobjekt als fachliche Mitte
- separater Masterzustand
- minimale SQLite-Persistenz fuer `devices`, `device_state_latest` und `master_status`
- Dashboard V1 mit zentraler Uebersicht und versteckter Detailseite pro Geraet

Nicht Teil dieser Stufe sind breite Komfortebenen wie Wetter, Diagramme, Logs-Konsole, breite Automationen oder eine grosse Command-Welt.

## Real bestaetigter Stand im Repo

Der aktuelle Stand ist im Repo nicht nur beschrieben, sondern direkt ablesbar:

- der reale Node-RED-Startpfad liegt in `server/docker-compose.yml`
- die aktive V1-Linie laeuft ueber `00_boot`, `05_dashboard_runtime`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `60_dashboard_overview`, `63_dashboard_device_detail` und `90_master_diag`
- `server/nodered/lib/dashboard_v1.js` liefert die Datenaufbereitung fuer Uebersicht und Detailseite
- `server/nodered/lib/device_store.js` haelt den Laufzeitkern und die schemaenge Persistenzlinie zusammen

Diese Doku beschreibt damit keinen Wunschstand, sondern den aktuell sichtbaren V1-Zuschnitt.

## Aktueller realer Zuschnitt

Der aktuelle Serverstand ist fachlich in vier Schichten zu lesen:

### 1. Start- und Laufzeitbasis

- `server/docker-compose.yml` startet Mosquitto, Node-RED und InfluxDB
- SQLite bleibt eine lokale Datei
- die Compose-Startlogik initialisiert Schema und kleine V1-Migrationen
- die aktiven Flow-Dateien werden beim Start zu `flows.json` zusammengebaut

### 2. Ingest, Store und Persistenz

- MQTT-Ingest fuer `meta`, `availability`, `state`, `event` und `ack`
- separater Masterpfad fuer `status` und `event`
- gemeinsamer Runtime-State in Node-RED
- zentrale SQLite-Writes aus derselben Handlerkette

### 3. Dashboard V1

- `http://localhost:1880/dashboard/`
- `http://localhost:1880/dashboard/geraet?device=<device_id>`

Das Dashboard V1 umfasst aktuell:
- eine zentrale Geraeteuebersicht
- eine versteckte Detailseite pro Geraet

Mehr ist oeffentlich fuer diese Stufe nicht zu behaupten.

### 4. Nachweis- und Laufzeitbasis

- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `tests/server/phase1_ingest_checkliste.md`

Die Datenbank und `flows.json` sind Laufzeitprodukte, keine zweite Architektur.

## Minimale Bausteine

### Laufzeitbasis

- `server/docker-compose.yml` ist der offizielle V1-Startpfad
- `server/.env.example` haelt die benoetigten Ports und Basiswerte
- `server/nodered/settings.js` bleibt die Basisdatei; die erweiterte Laufzeitkonfiguration entsteht beim Start

### Node-RED-Struktur

- `00_boot` initialisiert den gemeinsamen Runtime-State
- `05_dashboard_runtime` definiert Dashboard-Basis, Seiten und Gruppen
- `10_mqtt_ingest` abonniert die Pflicht-Topics und verteilt sie weiter
- `20_device_store` pflegt das Geraeteobjekt
- `30_sqlite_persist` schreibt den engen V1-Stand nach SQLite
- `60_dashboard_overview` liefert die zentrale Geraeteuebersicht
- `63_dashboard_device_detail` liefert die versteckte Detailseite pro Geraet
- `90_master_diag` haelt Masterstatus und Masterevents getrennt

### Fachliche Mitte

Pro Geraet wird eine einheitliche Struktur gepflegt:

- `identity`
- `meta`
- `availability`
- `state`
- `config`
- `last_event`
- `last_ack`
- `diagnostics`

Masterdaten laufen bewusst ausserhalb dieser Geraeteobjekte.

## MQTT-Pflichttopics in Phase 1

### Geraete

- `smarthome/device/+/meta`
- `smarthome/device/+/availability`
- `smarthome/device/+/state`
- `smarthome/device/+/event`
- `smarthome/device/+/ack`

### Master

- `smarthome/master/+/status`
- `smarthome/master/+/event`

## Warum dieser Zuschnitt richtig ist

Der V1-Stand bleibt klein, damit er lesbar und belastbar bleibt:

- Startpfad und Laufzeitwahrheit liegen an einer Stelle
- Store, Persistenz und Dashboard bauen auf derselben engen Linie auf
- die sichtbare UI bleibt auf Uebersicht und Detail beschraenkt
- spaetere Komfortebenen werden nicht als schon erreicht verkauft

## Bewusste Grenze dieser Stufe

Real vorhanden ist ein kleiner, belastbarer Serverstand mit Dashboard V1.

Nicht oeffentlich als aktueller V1-Hauptstand zu behandeln sind:
- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienflaeche
- breite Automationen
- Komfort-Commands als grosse Produktschicht
- eine breite UI ueber Uebersicht und Detailseite hinaus
