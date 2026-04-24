# Server

Bereichs-README fuer `server/`.
Dieser Stand bleibt bewusst klein und kontrolliert.

Nicht diese Datei:

- Gesamtarchitektur und Entscheidungen: `../docs/README.md`, `../docs/DECISIONS.md`
- belegter Gesamtstatus: `../docs/14_test_und_nachweisstand.md`

## Rolle der Serverbasis

Der Server liefert die kleine serverseitige Grundlage plus einen engen Dashboard-Schnitt:

- Mosquitto
- Node-RED mit kleinem FlowFuse-Dashboard
- SQLite als Snapshot-Persistenz

## Scope

- MQTT-Ingest fuer die engen aktuellen Themen
- Topic-Routing in Device- und Masterpfad
- gemeinsames Laufzeitobjekt pro Geraet
- separater Masterzustand
- minimale SQLite-Persistenz fuer `devices`, `device_state_latest` und `master_status`
- eine zentrale Geraeteuebersicht
- eine versteckte Detailseite pro Geraet
- Aktorsteuerung im engen aktuellen Rahmen

## Bewusst nicht Teil dieser Stufe

- Zeitreihen
- Diagramme
- Wetter
- volle Logs- oder MQTT-Konsole
- Commands als breite Komfortwelt
- Automationen
- SIM-spezifischer Ausbau
- grosse Konfigurationswelt

## Bereichsdoku

- `../docs/public/server/01_server_v1_ueberblick.md`
- `../docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `../docs/public/server/03_phase1_dateirollen.md`
- `flows/README.md`
- `db/README.md`
- `nodered/README.md`

## Start

Im Ordner `server/`:

1. optional `Copy-Item .env.example .env`
2. lokale Werte in `.env` setzen
3. `docker compose up --build -d`
4. `docker compose ps`

Der echte Startpfad liegt aktuell direkt in `docker-compose.yml`:
- Node-RED startet aus dem offiziellen `nodered/node-red:3.1`-Image.
- SQLite-Schema, kleine Bestandsschema-Migrationen, `flows.json` und die generierten Settings werden beim Containerstart inline vorbereitet.
- `nodered/settings.js` bleibt die Basisdatei, die Laufzeit erweitert sie beim Start um den benoetigten `functionGlobalContext`.

## Enger Vertragstest

Aus dem Repo-Root:

```powershell
.\tests\server\server_contract_smoke.ps1
```

Wenn der Stack bereits laeuft:

```powershell
.\tests\server\server_contract_smoke.ps1 -SkipStart
```

Der Test prueft den aktuellen Server-Vertrag fuer numerische `caps`, `button_flags`, ACK-bezogene Felder im aktuellen Snapshot und SQL-/Migrationsfehler beim Start.
Er nutzt `mosquitto_pub` im Compose-Service `mosquitto`; auf dem Host muss dieses Tool dafuer nicht lokal installiert sein.
Der Test ist ein enger Smoke-Test des Serververtrags, kein vollstaendiger End-to-End-Systemtest.

## Offizielle Serverdateien

- `docker-compose.yml`
- `.env.example`
- `sqlite/00_schema_phase1.sql`
- `nodered/settings.js`
- `nodered/flows/active/*.json`
- `nodered/lib/*.js`

## Lokal, nicht versioniert

- `.env`
- `config/.env`
- `config/mosquitto/config/` inklusive lokaler Auth-Dateien
- lokale Compose-Overrides

## Ehrliche Grenze

Wenn spaeter Zusatzfunktionen dazukommen sollen, muessen sie auf diesem kleinen Snapshot-, Anzeige- und Steuerkern aufbauen.
Der aktuelle Stand ist absichtlich nicht die Komfortebene.
