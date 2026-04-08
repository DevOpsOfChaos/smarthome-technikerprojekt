# Server V1 Überblick

## Ziel von Phase 1

Phase 1 zieht nur den fachlichen Kern hoch:

- MQTT-Ingest fuer Geraete und Master
- Topic-Routing auf kleine Handler
- gemeinsames Geraeteobjekt als fachliche Mitte
- enge offizielle Command-Ausgaenge fuer `net_erl` Relay 1 und Cover
- separater Masterzustand
- minimales SQLite-Schema als spaetere Persistenzbasis

Nicht Teil dieser Stufe sind breite UI, Diagramme, eine allgemeine Command-Welt, Wetter oder eine allgemeine Automationsplattform.

## Real bestaetigter Stand

Auf `main` ist dieser Zuschnitt nicht nur beschrieben, sondern auf dem realen Pfad nachgewiesen:

- die offiziellen Server-Einstiege `POST /api/phase1/net-erl/relay-1` und `POST /api/phase1/cover/command` sind aktiv
- fuer `net_erl_01` ist der echte HTTP -> MQTT -> Master -> Geraet -> ACK/State-Roundtrip belegt
- derselbe Minimalpfad ist nach Server-Restart erneut belegt
- derselbe Minimalpfad ist nach gezieltem Node-Recovery erneut belegt
- der Master-Recovery-Pfad ist fachlich ueber frischen Master-Status plus frische `availability`/`state` eingeordnet
- der fruehere Null-State nach Master-Recovery ist fuer den betroffenen Pfad behoben und per Hardware-Rerun des Master-Fixes bestaetigt

Die passenden Detailnachweise liegen in `PROTOKOLL/` und bauen auf derselben Phase-1-Linie auf. Diese Doku behauptet daher keinen Zukunftswunsch, sondern beschreibt den real bestaetigten Kern.

## Aktueller realer Zuschnitt

Der aktuelle Serverstand ist fachlich in vier Schichten zu lesen:

### 1. Technischer Kern

- MQTT-Ingest fuer `meta`, `availability`, `state`, `event`, `ack`
- separater Masterpfad fuer `status` und `event`
- gemeinsamer Runtime-State in Node-RED
- zentrale SQLite-Writes aus derselben Handlerkette

### 2. Offizieller neutraler Minimalpfad

- `POST /api/phase1/net-erl/relay-1`
- `POST /api/phase1/cover/command`

Diese beiden Pfade sind der offizielle obere Server-Minimalpfad.
Sie gehoeren zur neutralen Serverlinie.

### 3. Lokale Bedienflaeche

Zusaetzlich existiert real:
- `GET /device/<device_id>`
- `POST /api/phase1/cover/automation/<device_id>`

Diese Pfade bilden eine kleine lokale Cover-Detailseite mit zwei Zeit/Wert-Slots.
Sie sind real nutzbar, aber nicht Teil des neutralen oberen Vertrags.

### 4. Lokale Laufzeitdateien

- `server/sqlite/smarthome_phase1.db`
- `server/nodered/flows.json`
- `server/nodered/cover_automation.json`

Diese Dateien sind Teil des lokalen Laufzeitstands, nicht der neutralen Serveroberflaeche.

## Minimale Bausteine

### Laufzeitbasis

- `server/docker-compose.yml` startet nur Mosquitto, Node-RED und InfluxDB
- `server/.env.example` haelt nur die noetigen Ports und Basiswerte
- SQLite bleibt eine lokale Datei und braucht keinen eigenen Dienst
- die Node-RED-Startlogik baut die aktiven Flows zu `flows.json` zusammen und zieht das SQLite-Schema samt kleiner Phase-1-Migrationen hoch

### Node-RED-Struktur

- `00_boot` initialisiert den gemeinsamen Runtime-State
- `10_mqtt_ingest` abonniert nur die Pflicht-Topics und verteilt sie weiter
- `20_device_store` pflegt das Geraeteobjekt
- `40_command_minimal` baut die engen offiziellen Command-Einstiege fuer `net_erl` Relay 1 und Cover
- `41_cover_automation_detail` liefert die lokale Cover-Detailseite, den lokalen Save-Pfad und den kleinen Minutentick
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

### Geraete-Command Minimalpfad

- `POST /api/phase1/net-erl/relay-1`
- `POST /api/phase1/cover/command`
- MQTT-Publish auf `smarthome/device/<device_id>/command`
- Payload fuer `set_relay` auf `relay_1` oder fuer die Cover-Kommandos `open`, `close`, `stop`, `set_position`
- `set_position` respektiert serverseitig den letzten bekannten Zustand `cover_calibrated`

### Lokaler Cover-Bedienpfad

- `GET /device/<device_id>`
- `POST /api/phase1/cover/automation/<device_id>`
- lokale Speicherung in `server/nodered/cover_automation.json`
- Ausfuehrung ueber denselben Cover-Command-Baustein wie der offizielle Cover-Minimalpfad

Wichtig:
- dieser Pfad ist projektpraktisch und lokal
- er erweitert nicht den neutralen MQTT-Vertrag um Zeitfelder oder Scheduling-Topics

### Master

- `smarthome/master/+/status`
- `smarthome/master/+/event`

## Warum dieser Zuschnitt richtig ist

Der alte Fehler war nicht fehlende Features, sondern fehlende Disziplin. Phase 1 loest zuerst die fachliche Mitte:

- State bleibt Hauptwahrheit
- Event und ACK ergaenzen nur
- der Server erzeugt Commands nur ueber einen engen offiziellen Pfad
- der zusaetzliche lokale Cover-Bedienpfad bleibt bewusst ausserhalb dieses neutralen oberen Vertrags
- unbekannte Geraete duerfen bei validem Meta-, Availability- oder State-Pfad auto-angelegt werden
- partielle State-Nachrichten duerfen keine vorhandenen Felder loeschen

Das ist klein genug, um testbar zu bleiben, und stabil genug, um spaeter Persistenz, UI und Commands darauf aufzusetzen.

## Bewusste Grenze dieser Stufe

Real bestaetigt ist ein kleiner, belastbarer Serverstand.

Er ist mehr als nur Kern, weil:
- die offiziellen HTTP-Minimalpfade real aufrufbar sind
- eine kleine lokale Cover-Detailseite real existiert

Er ist aber noch keine breite Produktflaeche.
Nicht bestaetigt ist damit automatisch schon:

- eine breite Command-Matrix ueber die engen Relay- und Cover-Pfade hinaus
- eine voll ausgebaute UI fuer alle Geraeteklassen
- eine allgemeine Automationsplattform
- Timeseries- oder Komfortwelten ausserhalb des Phase-1-Kerns
