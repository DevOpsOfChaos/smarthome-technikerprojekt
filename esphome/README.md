# ESPHome MQTT-Linie

Diese Linie ist fuer dein ESPHome-Addon in Home Assistant vorbereitet. Die Geraete sprechen direkt MQTT mit dem Serververtrag. Es gibt hier keinen ESP-NOW-Master und keine MQTT-Bridge.

Die harte Regel: `device_id`, Topics und Payload-Felder bleiben kompatibel zur bisherigen Firmware, damit Node-RED nicht erkennen muss, ob ein altes Firmware-Geraet oder ein neues ESPHome-Geraet sendet.

## Import in Home Assistant

Kopiere den kompletten Ordner `esphome/` in die ESPHome-Konfiguration von Home Assistant, damit die relativen `packages/`-Includes funktionieren.

Danach kannst du die Dateien aus `devices/` im ESPHome-Dashboard importieren oder als neue YAML anlegen. Die benoetigten Secret-Werte liegen in Home Assistant in der zentralen ESPHome-Secret-Datei:

```yaml
wifi_ssid: <dein-wlan-name>
wifi_password: <dein-wlan-passwort>
mqtt_broker: <broker-host-oder-ip>
mqtt_username: <mqtt-benutzer>
mqtt_password: <mqtt-passwort>
```

Wenn dein MQTT-Broker keine Zugangsdaten nutzt, muessen die `username`/`password`-Zeilen in den Device-YAMLs entfernt oder passend ersetzt werden. ESPHome akzeptiert leere Secrets nicht immer sauber.

## Produktive Ersatz-YAMLs

| Datei | MQTT-Device-ID | Zweck |
|---|---:|---|
| `devices/esp_net_erl_light_example.yaml` | `NET-ERL-001` | Hall-Modul mit Relais, BME280, VEML7700 und PIR |
| `devices/esp_net_erl_hall_module_led_ring.yaml` | `NET-ERL-002` | Hall-Modul LED-Ring mit Relais, Sensorik, Button und LED-Ring |
| `devices/esp_net_sen_env_bme280_veml.yaml` | `NET-SEN-002` | Wetterstation mit BME280, VEML7700 und Regen-Digitalpin |
| `devices/esp_net_zrl_cover_example.yaml` | `NET-ZRL-002` | Rollladenmodul mit zwei Relais, lokalen Tastern, LEDs und Kalibrierung |
| `devices/bat_sen_window_contact_example.yaml` | `bat_sen_01` | Batterie-Fensterkontakt |
| `devices/bat_sen_rain_sensor_example.yaml` | `bat_sen_02` | Batterie-Regensensor |

Die Dateinamen sind noch teilweise historisch. Entscheidend fuer Node-RED ist nicht der Dateiname, sondern das MQTT-Feld `device_id` und der Topic-Pfad.

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

Home Assistant/ESPHome baut und flasht die Geraete. Node-RED sieht nur MQTT.

## Offene Hardware-Validierung

Die YAMLs sind vertraglich und pinseitig vorbereitet. Vor produktivem Einsatz musst du trotzdem je Board testen:

- Relaisrichtung und Active-Level
- Rollo-Auf/Ab-Verriegelung und Endlagen
- Taster-Pegel beim NET-ZRL
- Batterie-ADC-Kalibrierung
- Regen-ADC-Schwellwerte
- Sensoradressen bei BME/VEML/ENS160
