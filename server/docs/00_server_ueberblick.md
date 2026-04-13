# Server-├£berblick

Der Server ist in dieser Architektur nicht die Intelligenz der Feldger├ñte.
Er arbeitet oberhalb des Masters und ├╝bernimmt im aktuellen Stand:
- MQTT im Heimnetz
- Node-RED als Integrations- und kleine Dashboard-Schicht
- FlowFuse Dashboard als UI
- InfluxDB f├╝r Sensor-Zeitreihen
- SQLite f├╝r Ger├ñte- und Laufzeitdaten

Direkte Kommunikation mit Nodes findet nicht statt.
Alle Wege laufen ├╝ber den Master.

Der aktive ├Âffentliche Stand h├ñlt den Dashboard-Pfad bewusst eng:
eine Ger├ñte├╝bersicht als Einstieg und eine Detailseite pro Ger├ñt.
Wetter, Diagramme, Logs und Automationen sind in diesem Stand nicht Teil der aktiven Flowbasis.

Die Serverdarstellung soll langfristig nicht an fest verdrahteten Ger├ñtesonderf├ñllen h├ñngen, sondern aus `base_type`, `profile`, `capabilities`, `sensor_modules`, `actor_modules` und `config_schema` ableitbar werden.

## Aktueller belegter Serverbezug

- real lokal belegt: MQTT-Ingest mit SQLite-Ablage und Influx-Schreibpfad f├╝r numerische Sensorwerte
- Dashboard im ├Âffentlichen Repo bleibt ein enger Lesepfad auf Basis von `devices`, `device_state_latest` und `master_status`
- der belastbare Gesamtstand und offene Nachweise stehen in `../../docs/14_test_und_nachweisstand.md`

## Verbindliche Server-Dokumente

- `01_server_basisarchitektur.md`
  Zielbild, Module, Verantwortlichkeiten, Compose-Zielplattform und Wettereinordnung
- `../../docs/04_mqtt_topics.md`
  MQTT-Vertrag mit Topic-Baum, Payload-Regeln und Publikationsrechten
- `../db/README.md`
  Datenmodell f├╝r SQLite und InfluxDB
- `02_dashboard_konzept.md`
  Seiten, Kernfunktionen und UI-Regeln f├╝r FlowFuse Dashboard
- `../flows/README.md`
  kompakter Generatorbezug f├╝r die versionierten Node-RED-Flows

## Harte Grenzen

- Nur Heimnetz, keine Cloud-Pflicht und keine Home-Assistant-Abh├ñngigkeit.
- Docker Compose ist die Zielplattform f├╝r PC und sp├ñter Raspberry Pi.
- `smarthome/` bleibt das MQTT-Pr├ñfix.
- `availability` und `state` bleiben getrennt.
- Physische Ger├ñte werden technisch nur durch den Master bekannt gemacht.
