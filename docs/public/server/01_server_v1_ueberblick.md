# Server V1 Überblick

## Ziel von Phase 1

Phase 1 zieht nur den fachlichen Kern hoch:

- MQTT-Ingest fuer Geraete und Master
- Topic-Routing auf kleine Handler
- gemeinsames Geraeteobjekt als fachliche Mitte
- separater Masterzustand
- minimales SQLite-Schema als spaetere Persistenzbasis

Nicht Teil dieser Stufe sind breite UI, Diagramme, Commands, Wetter oder Automationen.

## Minimale Bausteine

### Laufzeitbasis

- `server/docker-compose.yml` startet nur Mosquitto, Node-RED und InfluxDB
- `server/.env.example` haelt nur die noetigen Ports und Basiswerte
- SQLite bleibt eine lokale Datei und braucht keinen eigenen Dienst

### Node-RED-Struktur

- `00_boot` initialisiert den gemeinsamen Runtime-State
- `10_mqtt_ingest` abonniert nur die Pflicht-Topics und verteilt sie weiter
- `20_device_store` pflegt das Geraeteobjekt
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

Der alte Fehler war nicht fehlende Features, sondern fehlende Disziplin. Phase 1 loest zuerst die fachliche Mitte:

- State bleibt Hauptwahrheit
- Event und ACK ergaenzen nur
- unbekannte Geraete duerfen bei validem Meta-, Availability- oder State-Pfad auto-angelegt werden
- partielle State-Nachrichten duerfen keine vorhandenen Felder loeschen

Das ist klein genug, um testbar zu bleiben, und stabil genug, um spaeter Persistenz, UI und Commands darauf aufzusetzen.
