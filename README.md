# smarthome-technikerprojekt

Öffentliches Hauptrepo für ein modulares Smart-Home-Technikerprojekt mit klarer Trennung zwischen Geräteschicht, Master und Server.

## Projektidee

Dieses Projekt baut kein beliebiges Smart-Home-Spielzeug und keine überladene Produktplattform, sondern eine **technisch saubere, nachvollziehbare und real testbare Systemlinie** für ein Technikerprojekt.

Im Mittelpunkt stehen:
- **dezentrale ESP32-Geräte** für Sensorik und Aktorik
- **ESP-NOW** als Funkweg zwischen Geräten und Master
- **ein Master** als einzige Brücke zwischen Funknetz und Server
- **MQTT** ausschließlich zwischen Master und Server
- **Node-RED** als aktueller Server- und Visualisierungskern
- eine Architektur, die **lehrerlesbar, modular und begründbar** bleibt

## Systemarchitektur in Kurzform

Das System ist bewusst in drei Schichten getrennt:

### 1. Geräteebene
Dezentrale Module auf ESP32-Basis übernehmen konkrete Sensor- und Aktorfunktionen.

Aktuelle Basistypen:
- `master`
- `net_erl`
- `net_zrl`
- `net_sen`
- `bat_sen`

Konkrete Geräte werden auf dieser Grundlage aufgebaut, statt für jeden Sonderfall eine neue Architektur einzuführen.

### 2. Master
Der Master ist die **einzige technische Brücke** zwischen ESP-NOW und MQTT.

Er ist bewusst **kein halber Server** und **keine zweite Logikplattform**, sondern:
- Funk-Gateway
- MQTT-Brücke
- einfache Projektion und Weiterleitung relevanter Zustände
- technischer Knotenpunkt für Commands, ACKs und Statuspfade

### 3. Server
Der Server bildet die aktuelle öffentliche Phase-1-Linie für:
- MQTT-Ingest
- Geräteobjekt und Zustandsmodell
- SQLite-basierte Latest-State- und Log-Grundlage
- Node-RED-basierte Bedien- und Diagnosepfade
- spätere Zeitreihen- und UI-Ableitungen

## Verbindliche technische Leitplanken

Dieses Repo folgt bewusst einigen harten Architekturregeln:

- **Nodes sprechen nicht direkt mit dem Server.**
- **MQTT ist kein zweiter Gerätebus**, sondern nur die Verbindung zwischen Master und Server.
- **Der Master bleibt die einzige Brücke.**
- **Sondergeräte werden nicht durch neue Server-Spezialarchitekturen erschlagen**, sondern sauber über Basistypen, Geräteebene und Fähigkeiten eingeordnet.
- **Kleine, begründete Änderungen** sind wichtiger als breite Umbauten.
- Öffentliche Doku und Code sollen **technisch sauber, nachvollziehbar und präsentierbar** bleiben.

## Aktueller öffentlicher Stand

Der aktuelle öffentliche Repo-Stand ist **kein fertiges Endprodukt**, sondern eine bewusst kontrollierte offizielle Linie.

Bereits sichtbar und nachweisbar sind unter anderem:
- öffentliche Projektdokumentation und Architekturgrundlinie
- Server-Phase-1-Basis mit MQTT-Ingest für Geräte und Master
- gemeinsames Geräteobjekt und separater Masterpfad
- minimales SQLite-Schema für Zustände und Nachweise
- minimaler Command-/ACK-Pfad für `net_erl_01`
- realer Roundtrip-Nachweis für den offiziellen Minimalpfad
- Restart-/Kaltstart-Nachweis desselben Pfads
- Recovery-Nachweise für Node- und Master-Pfad
- fachlich korrigiertes Master-Recovery-Gate
- nachgewiesener Master-Fix gegen den früheren Null-/Minimal-State nach Master-Recovery

Wichtig:
Das Repo zeigt damit bereits einen **real belastbaren technischen Kern**, ohne künstlich zu behaupten, dass das Gesamtsystem schon vollständig fertig wäre.

## Einstieg für Leser

Wer das Projekt strukturiert verstehen will, startet hier:

1. [Öffentliche Dokumentation](docs/public/README.md)
2. [Projektüberblick](docs/public/01_projektueberblick.md)
3. [Architektur und Kommunikation](docs/public/02_architektur_und_kommunikation.md)
4. [Aktueller Status und nächste Schritte](docs/public/03_aktueller_status_und_naechste_schritte.md)
5. [Server-Schnellstart Phase 1](docs/public/04_server_schnellstart_phase1.md)

Für den aktuellen Serverkern zusätzlich sinnvoll:
- [Server V1 Überblick](docs/public/server/01_server_v1_ueberblick.md)
- [Geräteobjekt und MQTT-Ingest](docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md)

## Repo-Struktur

- `docs/public/` – öffentliche technische Dokumentation und Einstiegspunkte
- `firmware/` – Firmware für Basistypen und konkrete Geräte
- `server/` – Node-RED-, MQTT- und Datenhaltungsstruktur
- `hardware/` – Hardware-Unterlagen, Schaltpläne und Layoutdaten
- `tests/` – Tests, Checklisten und technische Nachweise
- `PROTOKOLL/` – offizielle Projektprotokolle und Entwicklungsnachweise
- `docs/` – zusätzlicher Pages-/Doku-Einstieg für den öffentlichen Lesepfad

## Dokumentation und Nachweise

Ein zentrales Ziel dieses Projekts ist nicht nur Code, sondern **sauber belegbare technische Entwicklung**.

Darum spielen diese Bereiche eine wichtige Rolle:
- `docs/public/` für die offizielle technische Projektlinie
- `tests/` für strukturierte Testpfade
- `PROTOKOLL/` für reale Entwicklungs-, Diagnose- und Verifikationsnachweise

Damit bleibt der Fortschritt nicht nur behauptet, sondern im Repo nachvollziehbar.

## Öffentliche Projektseite

Für das Repo ist ein GitHub-Pages-Einstieg über `docs/` vorbereitet:
- [Projekt-Landingpage](docs/index.md)

## Repository

- GitHub-Repo: `DevOpsOfChaos/smarthome-technikerprojekt`

## Grundhaltung dieses Projekts

Dieses Projekt soll zeigen, dass ein Smart-Home-System auch im Rahmen einer Technikerarbeit **klar, modular, testbar und dokumentierbar** aufgebaut werden kann – ohne unnötige Komplexität, ohne Architektur-Show und ohne unsaubere Mischzustände.
