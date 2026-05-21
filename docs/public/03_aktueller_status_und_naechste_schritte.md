# Aktueller Status und nächste Schritte

## Öffentlicher Stand
Das öffentliche Repo enthält inzwischen eine klar erkennbare und real belegte technische Linie für:
- Projektstruktur
- öffentliche technische Dokumentation
- Serverbasis
- konkrete reale Gerätepfade
- Tests und Nachweise

Im Serverbereich ist aktuell eine belastbare Serverlinie sichtbar:
- MQTT-Ingest für Geräte und Master
- gemeinsames Geräteobjekt
- separater Masterpfad für Statusdiagnose
- minimales SQLite-Schema für aktuelle Zustände und Persistenz
- Dashboard mit Geräteübersicht und Detailpfad
- direktes MQTT-Gerätemodell als Grundlage für die ESPHome-Alternative

## Bereits real nachgewiesen
Folgende Punkte sind im Repo inzwischen nicht nur beschrieben, sondern durch Protokolle und Testläufe belegt:

### Server / Master / NET-ZRL
- realer Command-/ACK-/State-Roundtrip für `net_erl_01`
- erfolgreicher Restart-/Kaltstart-Nachweis des Minimalpfads
- erfolgreicher Node-Recovery-Nachweis für denselben Pfad
- Diagnose eines zu strengen Master-Recovery-Gates
- fachlich korrigierte Bewertung von Master-Recovery über frischen Master-Status plus frische `availability` und `state`
- Diagnose eines echten Master-Fehlers hinter Null-/Minimal-States nach Master-Recovery
- kleinster gezielter Fix im Master gegen dieses Wegsanitisieren valider State-Werte
- realer Hardware-Rerun des Master-Fixes auf `MASTER-001`
- belastbarer Dashboard-/`net_zrl`-Stand als öffentliche Serverlinie
- erneuter Latest-Flash-/Kommunikationsnachweis für `net_zrl`
- erfolgreicher enger Server-Contract-Smoke-Test für numerische `caps`, `button_flags`, ACK-`source` und SQL-/Migrationsstart

### NET-ERL Hall-Modul
- realer Gerätepfad `net_erl_hall_module`
- überprüfter Normalbetrieb gegen den echten Master
- real nachgewiesener Setup-Pfad über den konkreten Gerätepfad
- Provisioning und Rückkehr in den Normalmodus praktisch belegt

### NET-ERL Hall-Modul mit LED-Ring
- konkreter Gerätepfad `net_erl_hall_module_led_ring`
- Firmware-Umgebung mit Relais, Sensorik, Button und LED-Ring vorbereitet
- ESPHome-Alternative als `NET-ERL-020` vorbereitet

### NET-SEN Außensensor
- realer Gerätepfad `net_sen_weather_station`
- enger Device-ID-Fix auf protokollgültigen Gerätepfad
- realer Build-/Flash-/Kommunikationsnachweis
- BME280 real belegt
- VEML7700 real belegt
- digitaler Regenpfad real belegt
- MQTT-State real belegt
- Setup-Pfad real belegt

### ESPHome-Alternative
- eigener Ordner `esphome/` mit direkt angebundenen MQTT-Geräten
- kompatible Topics für `meta`, `availability`, `state`, `ack` und `command`
- vorbereitete YAMLs für Hall-Modul, LED-Ring-Modul, Wetterstation, Rollladenmodul, Fensterkontakt und Regensensor
- keine ESP-NOW-Strecke und kein Master in diesem Pfad
- offene Pflicht vor produktivem Einsatz: Board-, Pin-, Relais-, Sensor- und Batterievalidierung je Gerät

Damit ist der öffentliche Stand kein bloßer Strukturentwurf mehr, sondern ein bereits belastbar nachgewiesener technischer Kern aus Server, Master und konkreten Gerätepfaden.

## Was dieser Stand bedeutet
Der öffentliche Stand ist **nicht** als vollständiges Endsystem zu verstehen.
Er ist die aktuelle offizielle technische Linie, auf die weitere Arbeit sauber aufsetzen soll.

Wichtig ist dabei:
- zuerst fachliche Mitte stabil
- dann konkrete Gerätepfade sauber und ehrlich belegen
- dann Anzeige, Persistenz und zusätzliche Pfade kontrolliert erweitern
- Komfortfunktionen erst danach

## Bereits sichtbar im Repo
- grundlegende öffentliche Projektdokumentation
- technische Trennung von Geräteschicht, Master und Server
- aktueller Server für Ingest, Zustandsmodell, Snapshot-Persistenz und kleines Dashboard
- reale Nachweise für Hall-Modul, `net_zrl` und `net_sen`
- bestätigte Setup-Pfade für Hall-Modul und `net_sen`
- ESPHome-YAMLs als alternative direkte MQTT-Gerätelinie
- host-unabhängiger Server-Contract-Smoke-Test unter `tests/server/server_contract_smoke.ps1`
- offizielle Nachweis- und Teststruktur

## Bewusst noch nicht voll ausgebaut
- breite Server-UI
- Zeitreihenpfad als vollständige Sicht
- breite Command-Welt über den engen Kern hinaus
- Komfortfunktionen, Automationen oder Zusatzwelten
- endgültiger `bat_sen`-Hardwarepfad
- produktive Hardware-Freigabe aller ESPHome-YAMLs ohne Einzeltest

Das ist keine Schwäche, sondern bewusste Scope-Kontrolle.

## Nächste sinnvolle technische Schritte
1. belegten Hauptkern weiter konsolidieren
2. dokumentierte Gerätepfade und Nachweise sauber verdichten
3. ESPHome-Alternative pro Gerät real gegen Broker und Dashboard prüfen
4. `bat_sen` erst wieder öffnen, wenn die passende Versorgung real vorliegt
5. danach weitere Gerätepfade kontrolliert und nachweisbar ergänzen

## Öffentliche Repo-Perspektive
Für Außenstehende soll das Repo jetzt schon professionell lesbar sein:
- klare Projektlinie
- sauberer Einstieg
- nachvollziehbarer Unterschied zwischen belastbarem Stand und späteren Ausbaustufen
- reale Nachweise statt bloßer Implementierungsbehauptung

## Empfehlung für Leser
Wer den aktuellen technischen Kern verstehen will, liest als Nächstes:
- `04_server_schnellstart_phase1.md`
- `18_firmware_oder_esphome.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- `tests/server/phase1_ingest_checkliste.md`
- die zugehörigen Protokolle unter `PROTOKOLL/`
