# Öffentliche Dokumentation

Diese Dokumentation ist die öffentliche technische Projektlinie des Repos `smarthome-technikerprojekt`.

## Zweck
Diese Dateien sollen Außenstehenden schnell zeigen:
- was das Projekt ist
- wie die Systemarchitektur grundsätzlich aufgebaut ist
- welche technische Linie verbindlich gilt
- was aktuell bereits im öffentlichen Repo sichtbar umgesetzt und real belegt ist
- welche Firmware-Linien es gibt und wann welche sinnvoll ist
- welche Themen bewusst noch nicht öffentlich ausgebaut sind

## Öffentlicher Rahmen
Das öffentliche Repo enthält:
- technische Projektdokumentation
- Firmware- und Servercode der offiziellen Linie
- ESPHome-YAMLs als alternative MQTT-Linie
- Hardware-Unterlagen
- Tests und nachvollziehbare Nachweise

Nicht öffentlich ins Repo gehören:
- sensible Zugangsdaten
- private Arbeitsmaterialien
- interne Hilfsmittel oder interne Arbeitsprozesse

## Einstieg
1. `01_projektueberblick.md`
2. `02_architektur_und_kommunikation.md`
3. `03_aktueller_status_und_naechste_schritte.md`
4. `18_firmware_oder_esphome.md`
5. `04_server_schnellstart_phase1.md`

## Aktuell öffentlich belastbar
Der öffentliche Stand besteht inzwischen nicht mehr nur aus Architektur und Servergrundlage, sondern auch aus real belegten Gerätepfaden.

Belastbar sichtbar sind:
- Serverkern mit MQTT-Ingest, Geräteobjekt, Snapshot-Persistenz und Dashboard-Linie
- `net_zrl` als real belastbarer Server-/Gerätepfad im öffentlichen Kern
- `net_erl_hall_module` als real belegter konkreter Gerätepfad
- `net_erl_hall_module_led_ring` als vorbereiteter konkreter Gerätepfad
- real bestätigter Setup-Pfad für Hall-Modul
- `net_sen_weather_station` als real belegter konkreter Außensensorpfad
- real bestätigter Setup-Pfad für `net_sen`
- ESPHome-Alternative für `net_erl`, `net_sen`, `net_zrl` und `bat_sen`
- offizielle Entwicklungs- und Verifikationsnachweise unter `PROTOKOLL/`

## Teilbereiche im Repo
- `firmware/` Firmware für Basistypen und konkrete Geräte
- `esphome/` ESPHome-YAMLs für direkt angebundene MQTT-Geräte
- `server/` Serverstruktur für MQTT, Node-RED und Snapshot-Persistenz
- `hardware/` Hardware-Unterlagen und Schaltplan-/Layoutdaten
- `tests/` Tests, Checklisten und Nachweise
- `PROTOKOLL/` offizielle Projektprotokolle und Entwicklungsnachweise

## Wichtige Grundregel
Das Repo soll technisch sauber, erklärbar und lehrerlesbar bleiben.
Kleine, begründete Änderungen sind besser als breite unkontrollierte Umbauten.
