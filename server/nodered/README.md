# Node-RED V1

Dieser Ordner enthaelt die kleine Node-RED-Laufzeit fuer den engen Server-/Dashboard-V1-Pfad.

- `build-flows.js` baut `flows.json` aus `flows/active/`
- `lib/` enthaelt kleine Hilfsmodule fuer Routing, Handler und SQL-Bausteine
- `entrypoint.sh` initialisiert SQLite und baut die Flows beim Start neu

Aktiv enthalten:
- MQTT-Ingest
- Geraete- und Master-Store
- kleines FlowFuse-Dashboard mit Uebersicht und Detailseite

Bewusst nicht Teil dieser Stufe:
- Wetterpfad
- Diagramme
- Command-Komfort
- Automationen
- grosse Generatorlogik
