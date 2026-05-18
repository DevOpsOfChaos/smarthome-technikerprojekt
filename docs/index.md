---
layout: default
title: Smart-Home-Technikerprojekt
---

# Smart-Home-Technikerprojekt

Öffentliche Projektseite für das Repo **smarthome-technikerprojekt**.

Dieses Projekt dokumentiert ein lokales, modulares Smart-Home-System mit klarer Trennung zwischen Geräteschicht, Master und Server.

## Technische Grundlinie

- **ESP-NOW** zwischen dezentralen Geräten und Master
- **MQTT** nur zwischen Master und Server
- **Master** als einzige Brücke
- **Node-RED** als aktueller Server- und Visualisierungskern
- **saubere Basistypen** statt unkontrollierter Sonderfälle

## Öffentliche Dokumentation

Die öffentliche technische Doku liegt im Repo unter `docs/public/`.

Direkte Einstiege:

- [Projektüberblick](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/01_projektueberblick.md)
- [Architektur und Kommunikation](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/02_architektur_und_kommunikation.md)
- [Aktueller Status und nächste Schritte](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/blob/main/docs/public/03_aktueller_status_und_naechste_schritte.md)
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
- realer Gerätepfad `net_erl_hall_module`
- real bestätigter Setup-Pfad für Hall-Modul
- realer Gerätepfad `net_sen_weather_station`
- real bestätigter Setup-Pfad für `net_sen`
- reale Nachweise unter `PROTOKOLL/`

Wichtig:
Dies ist **keine fertige Produktplattform**, sondern ein kontrolliert wachsender, nachvollziehbarer Projektstand.

## Repo-Bereiche

- [Öffentliche Doku](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/docs/public)
- [Firmware](https://github.com/DevOpsOfChaos/smarthome-technikerprojekt/tree/main/firmware)
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
