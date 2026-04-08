# Server Schnellstart Phase 1

## Ziel dieses Dokuments
Diese Anleitung beschreibt den kleinsten sinnvollen öffentlichen Einstieg in den aktuellen Server-Phase-1-Stand.

## Enthaltene Bausteine
Der aktuelle Phase-1-Stand umfasst:
- Mosquitto
- Node-RED
- InfluxDB
- aktive Phase-1-Flows
- minimales SQLite-Schema im Repo
- einen engen offiziellen Minimal-Command-Pfad für `net_erl_01` Relay 1
- einen engen offiziellen Minimal-Command-Pfad fuer Cover-Devices

## Wichtige Einordnung
Dieser Stand ist ein technischer Kernstart, kein vollständiger Produktivstand.

Phase 1 soll vor allem zeigen:
- MQTT-Ingest funktioniert strukturell
- Geräte- und Masterdaten werden getrennt behandelt
- der Zustandskern ist sauber vorbereitet
- der enge Minimalpfad für Command, ACK und State ist nachvollziehbar aufgebaut

## Relevante Dateien
- `server/docker-compose.yml`
- `server/.env.example`
- `server/sqlite/00_schema_phase1.sql`
- `server/nodered/flows/active/00_boot.json`
- `server/nodered/flows/active/10_mqtt_ingest.json`
- `server/nodered/flows/active/20_device_store.json`
- `server/nodered/flows/active/30_sqlite_persist.json`
- `server/nodered/flows/active/40_command_minimal.json`
- `server/nodered/flows/active/90_master_diag.json`
- `tests/server/phase1_ingest_checkliste.md`

## Startablauf
1. in den Ordner `server/` wechseln
2. lokale Umgebungswerte aus `server/.env.example` übernehmen oder anpassen
3. `docker compose up -d` ausführen
4. Node-RED unter `http://localhost:1880` öffnen
5. Ingest-Checkliste aus `tests/server/phase1_ingest_checkliste.md` verwenden

## Erwartung an diesen Stand
Nach dem Start soll vor allem nachvollziehbar sein:
- welche MQTT-Themen Phase 1 verarbeitet
- wie Geräte intern modelliert werden
- dass der Master getrennt vom normalen Gerätepfad geführt wird
- wie die engen Minimalpfade fuer Relay und Cover aufgebaut sind
- wie SQLite aus derselben fachlichen Handlerkette beschrieben wird

## Bereits öffentlich belegter Minimalpfad
Der öffentliche Stand enthält bereits einen real nachgewiesenen engen Bedienpfad für genau einen Fall:
- HTTP `POST /api/phase1/net-erl/relay-1`
- MQTT-Command auf dem offiziellen Device-Topic
- Rücklauf von ACK und State über den realen Master-/Gerätepfad
- passende SQLite-Belege im Server

Dieser Pfad ist bewusst eng gehalten und dient als belastbare Phase-1-Basis, nicht als fertige allgemeine Command-Welt.

Zusaetzlich gibt es jetzt einen engen neutralen Cover-Einstieg:
- HTTP `POST /api/phase1/cover/command`
- MQTT-Command `open`, `close`, `stop` oder `set_position` auf `smarthome/device/<device_id>/command`
- serverseitige Respektierung von `cover_calibrated` fuer `set_position`

## Was noch nicht erwartet werden sollte
- vollständige Serveroberfläche
- komplette Verlaufs- und Logansichten
- breite allgemeine Bedienpfade für alle Geräteklassen
- umfassende Komfortfunktionen

## Weiterführende Dateien
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`
- `PROTOKOLL/` für die realen Roundtrip-, Restart- und Recovery-Nachweise
