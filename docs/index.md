---
layout: default
title: Smart-Home-Technikerprojekt
---

# Smart-Home-Technikerprojekt

Öffentliche Projektseite für das Repo **smarthome-technikerprojekt**.

Dieses Projekt zeigt ein lokales, modulares Smart-Home-System auf ESP32-Basis. Es richtet sich an Leser, die verstehen wollen, wie Sensoren, Aktoren, Funkstrecke, Server, Dashboard und Nachweise in einem Technikerprojekt sauber zusammengefuehrt werden koennen.

## Worum es geht

Das System besteht aus:

- **ESP32-Geraeten** fuer Sensorik und Aktorik
- **einem Serverkern** mit MQTT-Ingest, Node-RED-Dashboard und SQLite-Snapshot-Persistenz
- **klaren Geraetevertraegen** fuer `meta`, `availability`, `state`, `event`, `ack` und `command`
- **nachvollziehbaren Tests und Protokollen**, statt nur Architekturversprechen

Es gibt zwei nutzbare Firmware-Linien:

| Linie | Kurzbeschreibung | Geeignet fuer |
|---|---|---|
| Eigene Firmware | ESP-NOW zwischen Geraeten und Master, MQTT nur zwischen Master und Server | maximale Kontrolle, eigener Funkpfad, saubere Technikerarbeits-Architektur |
| ESPHome-Alternative | Geraete sprechen direkt per MQTT mit demselben Serververtrag | Nutzer, die lieber mit ESPHome, Home Assistant und YAML arbeiten |

Der gemeinsame Punkt ist der Serververtrag. Node-RED soll moeglichst denselben Geraetezustand sehen, egal ob ein Geraet ueber die eigene Firmware-Linie oder ueber ESPHome angebunden wird.

## Aktuelle technische Linie

Die offizielle Hauptlinie bleibt bewusst schlank:

- **ESP-NOW** zwischen dezentralen Geraeten und Master
- **MQTT** zwischen Master und Server
- **Master** als Bruecke fuer die eigene Firmware-Linie
- **Node-RED** als aktueller Server- und Visualisierungskern
- **Basistypen** statt unkontrollierter Sonderfaelle

Die ESPHome-Linie ist eine bewusst getrennte Alternative. Sie nutzt keinen ESP-NOW-Master, sondern direktes MQTT, bleibt aber beim Topic- und Payload-Vertrag kompatibel.

## Öffentliche Dokumentation

Die öffentliche technische Doku liegt im Repo unter `docs/public/`.

Direkte Einstiege:

- [Projektüberblick](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/01_projektueberblick.md)
- [Architektur und Kommunikation](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/02_architektur_und_kommunikation.md)
- [Aktueller Status und nächste Schritte](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/03_aktueller_status_und_naechste_schritte.md)
- [Eigene Firmware oder ESPHome?](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/18_firmware_oder_esphome.md)
- [Server-Schnellstart Phase 1](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/04_server_schnellstart_phase1.md)
- [Server V1 Überblick](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/server/01_server_v1_ueberblick.md)
- [Geräteobjekt und MQTT-Ingest](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md)

## Aktuell bestätigter öffentlicher Stand

Der aktuelle öffentliche Stand zeigt bereits eine belastbare Linie aus Serverkern **und** konkreten real belegten Gerätepfaden.

Belastbar sichtbar sind:
- MQTT-Ingest für Geräte und Master
- gemeinsames Geräteobjekt
- minimales SQLite-Fundament
- Dashboard-V1-Grundlinie
- `net_zrl_shutter_module` als Rolladenpfad
- realer Gerätepfad `net_erl_hall_module`
- vorbereiteter Gerätepfad `net_erl_hall_module_led_ring`
- real bestätigter Setup-Pfad für Hall-Modul
- realer Gerätepfad `net_sen_weather_station`
- real bestätigter Setup-Pfad für `net_sen`
- vorbereitete ESPHome-YAMLs fuer `net_erl`, `net_sen`, `net_zrl` und `bat_sen`
- reale Nachweise unter `PROTOKOLL/`

Wichtig:
Dies ist **keine fertige Produktplattform**, sondern ein kontrolliert wachsender, nachvollziehbarer Projektstand. Die eigene Firmware-Linie ist die Architektur-Hauptlinie; ESPHome ist die praktische Alternative fuer Anwender, die den Serververtrag nutzen moechten, aber ihre Geraete lieber ueber Home Assistant/ESPHome bauen.

## Repo-Bereiche

- [Öffentliche Doku](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/docs/public)
- [Firmware](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/firmware)
- [ESPHome-Alternative](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/esphome)
- [Server](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/server)
- [Hardware](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/hardware)
- [Tests](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/tests)
- [Protokolle / Nachweise](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/PROTOKOLL)

## Hinweise

- Das öffentliche Repo bleibt bewusst technisch und systemneutral formuliert.
- Interne oder nichtöffentliche Arbeitsstände gehören nicht hierher.
- Kleine, nachvollziehbare Schritte sind ausdrücklich wichtiger als breite unkontrollierte Umbauten.

## Repository

- [GitHub-Repository öffnen](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt)
