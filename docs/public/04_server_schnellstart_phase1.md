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

## Wichtige Einordnung
Dieser Stand ist ein technischer Kernstart, kein vollständiger Produktivstand.

Phase 1 soll vor allem zeigen:
- MQTT-Ingest funktioniert strukturell
- Geräte- und Masterdaten werden getrennt behandelt
- der Zustandskern ist sauber vorbereitet

## Relevante Dateien
- `server/docker-compose.yml`
- `server/.env.example`
- `server/sqlite/00_schema_phase1.sql`
- `server/nodered/flows/active/00_boot.json`
- `server/nodered/flows/active/10_mqtt_ingest.json`
- `server/nodered/flows/active/20_device_store.json`
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

## Was noch nicht erwartet werden sollte
- vollständige Serveroberfläche
- komplette Verlaufs- und Logansichten
- fertiger Bedienpfad für Gerätekommandos
- umfassende Komfortfunktionen

## Weiterführende Dateien
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `docs/public/server/03_phase1_dateirollen.md`