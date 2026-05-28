# Node-RED

Dieser Ordner enthaelt die kleine Node-RED-Laufzeit fuer den engen Server-/Dashboard-Pfad.

- `flows/active/` enthaelt die produktiven Flow-Fragmente
- `lib/` enthaelt kleine Hilfsmodule fuer Routing, Handler und SQL-Bausteine
- `settings.js` ist die Basis fuer die Node-RED-Settings
- der echte Startpfad liegt aktuell in `../docker-compose.yml`: dort werden SQLite, Bestandsschema-Migrationen, `flows.json` und die erweiterten Settings beim Containerstart inline erzeugt

Aktiv enthalten:
- MQTT-Ingest
- Geräte- und Master-Store
- kleines FlowFuse-Dashboard mit Übersicht und Detailseite
- Automatisierungsseite mit SQLite-Persistenz und minütlichem Runner

Bewusst nicht Teil dieser Stufe:
- Wetterpfad
- Diagramme
- Command-Komfort
- grosse Generatorlogik
