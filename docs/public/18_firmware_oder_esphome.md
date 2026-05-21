# Eigene Firmware oder ESPHome?

## Kurzfassung

Dieses Projekt bietet zwei Wege, um ESP32-Geräte an den Server anzubinden.

Die **eigene Firmware** ist die Architektur-Hauptlinie des Technikerprojekts. Sie nutzt ESP-NOW zwischen Gerät und Master. Der Master übersetzt zwischen ESP-NOW und MQTT. Das ist technisch kontrollierter, besser erklärbar und näher an der eigentlichen Projektarchitektur.

Die **ESPHome-Alternative** ist für Nutzer gedacht, die lieber mit ESPHome, Home Assistant und YAML arbeiten. Diese Geräte sprechen direkt MQTT mit dem Server. Der Master wird dabei nicht genutzt. Der Vorteil ist ein schnellerer Einstieg und eine bekannte Werkzeugkette.

Beide Linien sollen denselben Serververtrag bedienen. Der Server interessiert sich vor allem dafür, dass die MQTT-Topics und Payload-Felder stimmen.

## Gemeinsamer Serververtrag

Der Server erwartet pro Gerät klar strukturierte MQTT-Nachrichten:

- `smarthome/device/<device_id>/meta`
- `smarthome/device/<device_id>/availability`
- `smarthome/device/<device_id>/state`
- `smarthome/device/<device_id>/event`
- `smarthome/device/<device_id>/ack`
- `smarthome/device/<device_id>/command`

Für die Anzeige ist `state` die Hauptwahrheit. `meta` beschreibt das Gerät, `availability` zeigt die Erreichbarkeit, `ack` bestätigt Befehle und `event` ergänzt Zustandsänderungen.

## Weg 1: Eigene Firmware

Die eigene Firmware liegt unter `firmware/`.

Sie ist sinnvoll, wenn du:
- den ESP-NOW-Funkpfad selbst kontrollieren willst
- den Master als klare Brücke zwischen Geräteebene und Server erklären möchtest
- Verhalten, Timing, Provisionierung und Fehlerbehandlung selbst in C++ steuern willst
- die Technikerarbeit stärker als eigenes eingebettetes System zeigen willst
- Gerätefunktionen bewusst in Basistyp und konkrete Geräteschicht trennen möchtest

Aktuelle Basistypen und Geräte:

| Bereich | Zweck |
|---|---|
| `master_firmware` | Brücke zwischen ESP-NOW und MQTT |
| `net_erl` | netzbetriebener Relais-/Lichttyp |
| `net_zrl` | netzbetriebener Rollladentyp mit zwei Relais |
| `net_sen` | netzbetriebener Sensortyp |
| `bat_sen` | batteriebetriebener Sensortyp mit Sleep-Konzept |
| `net_erl_hall_module` | Hall-Modul mit Relais, BME280, VEML7700 und PIR |
| `net_erl_hall_module_led_ring` | Hall-Modul mit Relais, Sensorik, Button und LED-Ring |
| `net_sen_weather_station` | Wetterstation mit BME280, VEML7700 und Regen-Digitalpfad |
| `net_zrl_shutter_module` | Rollladenmodul mit zwei Relais, lokalen Tastern und Kalibrierung |
| `bat_sen_window_contact` | Batterie-Fensterkontakt |
| `bat_sen_rain_sensor` | Batterie-Regensensor |

Der Preis dieser Linie: mehr eigener Code, mehr Verantwortung beim Testen und mehr Aufwand pro Gerät. Die Stärke liegt genau darin, dass das Verhalten wirklich im eigenen System liegt und nicht in einer fertigen ESPHome-Abstraktion verschwindet.

## Weg 2: ESPHome-Alternative

Die ESPHome-Alternative liegt unter `esphome/`.

Sie ist sinnvoll, wenn du:
- ESPHome oder Home Assistant bereits nutzt
- Geräte schnell über YAML bauen und flashen willst
- direktes WLAN/MQTT statt ESP-NOW akzeptierst
- den Master nicht brauchst
- den Serververtrag des Projekts trotzdem verwenden möchtest

Vorbereitete ESPHome-Geräte:

| Datei | MQTT-Device-ID | Zweck |
|---|---:|---|
| `devices/net_erl_hall_module.yaml` | `NET-ERL-010` | Hall-Modul mit Relais, BME280, VEML7700 und PIR |
| `devices/net_erl_hall_module_led_ring.yaml` | `NET-ERL-020` | Hall-Modul mit Relais, Sensorik, Button und LED-Ring |
| `devices/net_sen_weather_station.yaml` | `NET-SEN-020` | Wetterstation mit BME280, VEML7700 und Regen-Digitalpin |
| `devices/net_zrl_shutter_module.yaml` | `NET-ZRL-020` | Rollladenmodul mit zwei Relais, lokalen Tastern, LEDs und Kalibrierung |
| `devices/bat_sen_window_contact.yaml` | `bat_sen_010` | Batterie-Fensterkontakt |
| `devices/bat_sen_rain_sensor.yaml` | `bat_sen_020` | Batterie-Regensensor |

Der Preis dieser Linie: kein ESP-NOW, kein Masterpfad, weniger eigene Kontrolle über die unteren Abläufe. Dafür ist der Einstieg pragmatischer und für ESPHome-Nutzer deutlich näher an ihrer gewohnten Arbeitsweise.

## Entscheidungshilfe

| Frage | Eher eigene Firmware | Eher ESPHome |
|---|---|---|
| Soll ESP-NOW Teil des Projekts bleiben? | Ja | Nein |
| Soll der Master als klare Brücke gezeigt werden? | Ja | Nein |
| Ist schnelle YAML-Konfiguration wichtiger als eigener Gerätecode? | Nein | Ja |
| Soll Home Assistant/ESPHome die Build- und Flash-Werkzeuge liefern? | Nicht zentral | Ja |
| Ist maximale Kontrolle über Timing, Provisionierung und ACK-Logik wichtig? | Ja | Teilweise |
| Soll der Server möglichst denselben Vertrag sehen? | Ja | Ja |

## Praktische Empfehlung

Für die Technikerarbeit ist die eigene Firmware-Linie die stärkere Hauptargumentation: Sie zeigt Architektur, Embedded-Logik, Funkstrecke, Masterrolle, Serververtrag und Nachweisführung aus einem Guss.

Für Nutzer, die das System praktisch nachbauen oder in eine vorhandene ESPHome-Umgebung einordnen möchten, ist die ESPHome-Linie der leichtere Einstieg. Sie sollte aber nicht als Beweis für die ESP-NOW-/Master-Architektur verkauft werden. Das wäre fachlich unsauber.

## Was vor produktivem Einsatz zu prüfen ist

Unabhängig von der Linie müssen reale Geräte geprüft werden:

- Pinbelegung pro Board
- Relaisrichtung und Active-Level
- Verriegelung bei Rollladen-Auf/Ab
- Endlagen und Kalibrierung
- Sensoradressen und Messwerte
- Batterie-ADC und Schwellwerte
- MQTT-Topics und Payload-Felder
- Dashboard-Anzeige und ACK-Verhalten

Das Projekt ist bewusst so dokumentiert, dass Fortschritt belegt werden kann. Behauptete Gerätepfade ohne Testnachweis sind kein belastbarer Stand.
