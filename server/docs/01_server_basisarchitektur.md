# Server Basisarchitektur

## Dienste

- Mosquitto als MQTT-Broker
- Node-RED als Integrations- und Dashboard-Schicht
- InfluxDB f├╝r Sensor-Zeitreihen
- SQLite f├╝r operative Serverdaten

## Broker-Betrieb

- `server/config/mosquitto/mosquitto.conf` bleibt die einzige versionierte Broker-Vorlage
- der Repo-Default bleibt anonym startbar, damit keine lokalen Zugangsdaten ins Repo m├╝ssen
- lokale Passwortpflicht wird nur ├╝ber ignorierte Dateien unter `server/config/mosquitto/config/` aktiviert
- MQTT-Vertrag, Topic-Baum und Payload-Semantik bleiben dabei unver├ñndert

## Datenfluss

1. Master publiziert unter `smarthome/master/<master_id>/...` und `smarthome/device/<device_id>/...`
2. Node-RED normalisiert genau diese Topics in `scope`, `entityId`, `channel` und Payload
3. unbekannte Master und Nodes werden sofort in SQLite angelegt
4. `availability`, `state`, `event` und `ack` bleiben im Laufzeitobjekt getrennt und werden im aktuellen SQLite-Schnitt als Roh-JSON gehalten
5. numerische Sensorwerte aus `device/.../state` koennen parallel nach InfluxDB geschrieben werden
6. das Dashboard liest im aktuellen engen Serverschnitt nur aus SQLite und zeigt Geraeteuebersicht plus Geraetedetail

## Realer Nachweisstand dieser Architektur

- im Repo real belegt: MQTT-Ingest, SQLite-Ablage und der kleine Dashboard-Lesepfad fuer Geraete
- das Dashboard liest `availability` kanonisch aus `device_state_latest.availability_json` und zeigt `state`, `meta`, `event`, `ack` und `diagnostics` roh auf der Detailseite
- offen bleiben weitergehende Command-Pfade, Diagramme, Wetter, Logs, Automationen, weitere Basisgeraete und ein vollst├ñndiger Gesamtprojektnachweis

## Grenzen

- keine Zusatzdienste au├ƒer Mosquitto, Node-RED und InfluxDB
- keine Home-Assistant-Abh├ñngigkeit
- kein Wetterpfad im aktiven ├Âffentlichen Dashboard-Schnitt
- keine Diagramm-, Log- oder Automationsfl├ñchen im aktiven ├Âffentlichen Dashboard-Schnitt
- keine konkurrierende Server-Timeout-Logik f├╝r Node-Onlinezustand; der Master bleibt autoritativ
- kein Legacy-Support f├╝r `smarthome/master/status` ohne `<master_id>`
