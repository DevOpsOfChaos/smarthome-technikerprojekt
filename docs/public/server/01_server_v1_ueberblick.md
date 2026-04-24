# Server Überblick

## Ziel des aktuellen Servers

Der aktuelle Server führt den kleinen kontrollierten Serverkern:

- MQTT-Ingest für Geräte und Master
- Topic-Routing auf kleine Handler
- gemeinsames Geräteobjekt als fachliche Mitte
- separater Masterzustand
- minimale SQLite-Persistenz für `devices`, `device_state_latest` und `master_status`
- Dashboard mit zentraler Übersicht und versteckter Detailseite pro Gerät

Nicht Teil dieser Stufe sind breite Komfortebenen wie Wetter, Diagramme, Logs-Konsole, breite Automationen oder eine große Command-Welt.

## Real bestätigter Stand im Repo

Der aktuelle Stand ist im Repo nicht nur beschrieben, sondern direkt ablesbar:

- der reale Node-RED-Startpfad liegt in `server/docker-compose.yml`
- die aktive Serverlinie läuft über `00_boot`, `05_dashboard_runtime`, `10_mqtt_ingest`, `20_device_store`, `30_sqlite_persist`, `60_dashboard_overview`, `63_dashboard_device_detail` und `90_master_diag`
- `server/nodered/lib/dashboard_v1.js` liefert die Datenaufbereitung für Übersicht und Detailseite
- `server/nodered/lib/device_store.js` hält den Laufzeitkern und die enge Persistenzlinie zusammen

Diese Doku beschreibt damit keinen Wunschstand, sondern den aktuell sichtbaren Serverzuschnitt.

## Aktueller realer Zuschnitt

Der aktuelle Serverstand ist fachlich in vier Schichten zu lesen:

### 1. Start- und Laufzeitbasis

- `server/docker-compose.yml` startet Mosquitto und Node-RED
- SQLite bleibt eine lokale Datei
- die Compose-Startlogik initialisiert Schema und kleine Bestandsschema-Migrationen
- die aktiven Flow-Dateien werden beim Start zu `flows.json` zusammengebaut

### 2. Ingest, Store und Persistenz

- MQTT-Ingest für `meta`, `availability`, `state`, `event` und `ack`
- separater Masterpfad für `status`
- gemeinsamer Runtime-State in Node-RED
- zentrale SQLite-Writes aus derselben Handlerkette
- Snapshot-Persistenz statt Verlaufs- oder Zeitreihenhaltung

### 3. Dashboard

- `http://localhost:1880/dashboard/`
- `http://localhost:1880/dashboard/geraet?device=<device_id>`

Das Dashboard umfasst aktuell:
- eine zentrale Geräteübersicht
- eine versteckte Detailseite pro Gerät

Mehr ist öffentlich für diese Stufe nicht zu behaupten.

### 4. Nachweis- und Laufzeitbasis

- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `tests/server/phase1_ingest_checkliste.md`

Die Datenbank und `flows.json` sind Laufzeitprodukte, keine zweite Architektur.

## Minimale Bausteine

### Laufzeitbasis

- `server/docker-compose.yml` ist der offizielle Startpfad
- `server/.env.example` hält die benötigten Ports und Basiswerte
- `server/nodered/settings.js` bleibt die Basisdatei; die erweiterte Laufzeitkonfiguration entsteht beim Start

### Node-RED-Struktur

- `00_boot` initialisiert den gemeinsamen Runtime-State
- `05_dashboard_runtime` definiert Dashboard-Basis, Seiten und Gruppen
- `10_mqtt_ingest` abonniert die Pflicht-Topics und verteilt sie weiter
- `20_device_store` pflegt das Geräteobjekt
- `30_sqlite_persist` schreibt den engen aktuellen Stand nach SQLite
- `60_dashboard_overview` liefert die zentrale Geräteübersicht
- `63_dashboard_device_detail` liefert die versteckte Detailseite pro Gerät
- `90_master_diag` hält Masterstatus getrennt

### Fachliche Mitte

Pro Gerät wird eine einheitliche Struktur gepflegt:

- `identity`
- `meta`
- `availability`
- `state`
- `config`
- `last_event`
- `last_ack`
- `diagnostics`

Masterdaten laufen bewusst außerhalb dieser Geräteobjekte.

## MQTT-Pflichttopics

### Geräte

- `smarthome/device/+/meta`
- `smarthome/device/+/availability`
- `smarthome/device/+/state`
- `smarthome/device/+/event`
- `smarthome/device/+/ack`

### Master

- `smarthome/master/+/status`

## Warum dieser Zuschnitt richtig ist

Der Serverstand bleibt klein, damit er lesbar und belastbar bleibt:

- Startpfad und Laufzeitwahrheit liegen an einer Stelle
- Store, Persistenz und Dashboard bauen auf derselben engen Linie auf
- die sichtbare UI bleibt auf Übersicht und Detail beschränkt
- spätere Komfortebenen werden nicht als schon erreicht verkauft

## Bewusste Grenze

Real vorhanden ist ein kleiner, belastbarer Serverstand mit Dashboard.

Nicht öffentlich als aktueller Hauptstand zu behandeln sind:
- Wetter
- Diagramme
- Logs- oder MQTT-Konsole als Bedienfläche
- breite Automationen
- Komfort-Commands als große Produktschicht
- eine breite UI über Übersicht und Detailseite hinaus
