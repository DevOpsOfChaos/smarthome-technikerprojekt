# ESPHome-Alternative mit direktem MQTT

Diese Linie ist fuer Nutzer gedacht, die ihre ESP32-Geraete lieber mit ESPHome und Home Assistant bauen. Die Geraete sprechen direkt MQTT mit dem Serververtrag. Es gibt hier keinen ESP-NOW-Master und keine MQTT-Bridge.

Die harte Regel: Topics und Payload-Felder bleiben kompatibel zur bisherigen Firmware, damit Node-RED nur den MQTT-Vertrag sieht.

Diese Linie ist eine Alternative zur eigenen Firmware, nicht die Architektur-Hauptlinie des Technikerprojekts. Die Hauptlinie bleibt `firmware/` mit ESP-NOW, Master und MQTT-Bruecke.

## Import in Home Assistant

Kopiere den kompletten Ordner in die passende Home-Assistant-Konfiguration, damit die relativen `packages/`-Includes funktionieren.

Danach kannst du die Dateien aus `devices/` importieren oder als neue YAML anlegen. Die benoetigten Secret-Werte liegen in Home Assistant in der zentralen Secret-Datei:

```yaml
wifi_ssid: <dein-wlan-name>
wifi_password: <dein-wlan-passwort>
mqtt_broker: <broker-host-oder-ip>
mqtt_username: <mqtt-benutzer>
mqtt_password: <mqtt-passwort>
```

Wenn dein MQTT-Broker keine Zugangsdaten nutzt, muessen die `username`/`password`-Zeilen in den Device-YAMLs entfernt oder passend ersetzt werden.

## Produktive Ersatz-YAMLs

| Datei | MQTT-Device-ID | Zweck |
|---|---:|---|
| `devices/net_erl_hall_module.yaml` | `NET-ERL-010` | Hall-Modul mit Relais, BME280, VEML7700 und PIR |
| `devices/net_erl_hall_module_led_ring.yaml` | `NET-ERL-020` | Hall-Modul LED-Ring mit Relais, Sensorik, Button und LED-Ring |
| `devices/net_sen_weather_station.yaml` | `NET-SEN-020` | Wetterstation mit BME280, VEML7700 und Regen-Digitalpin |
| `devices/net_zrl_shutter_module.yaml` | `NET-ZRL-020` | Rollladenmodul mit zwei Relais, lokalen Tastern, LEDs und Kalibrierung |
| `devices/bat_sen_window_contact.yaml` | `bat_sen_010` | Batterie-Fensterkontakt |
| `devices/bat_sen_rain_sensor.yaml` | `bat_sen_020` | Batterie-Regensensor |

Entscheidend fuer Node-RED ist der MQTT-Vertrag: Topic-Pfad und Payload-Felder.

## MQTT-Vertrag

Alle Geraete senden direkt:

- `smarthome/device/<device_id>/meta` retained
- `smarthome/device/<device_id>/availability` retained
- `smarthome/device/<device_id>/state` retained
- `smarthome/device/<device_id>/ack` nicht retained

Aktoren hoeren auf:

- `smarthome/device/<device_id>/command`

Commands brauchen `request_id`. ACKs geben dieselbe `request_id` zurueck.

## Wichtige Abgrenzung

Diese Linie nutzt:

- kein ESP-NOW
- keinen Master
- keine `master_compat`-Bridge
- keine internen Arbeitsmittel

Home Assistant baut und flasht die Geraete. Node-RED sieht nur MQTT.

## Offene Hardware-Validierung

Die YAMLs sind vertraglich und pinseitig vorbereitet. Vor produktivem Einsatz musst du trotzdem je Board testen:

- Relaisrichtung und Active-Level
- Rollo-Auf/Ab-Verriegelung und Endlagen
- Taster-Pegel beim NET-ZRL
- Batterie-ADC-Kalibrierung
- Regen-ADC-Schwellwerte
- Sensoradressen bei BME/VEML/ENS160
