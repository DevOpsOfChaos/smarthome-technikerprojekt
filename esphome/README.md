# Alternative MQTT-Linie

Diese Linie ist fuer direkt angebundene MQTT-Geraete vorbereitet. Die Geraete sprechen direkt MQTT mit dem Serververtrag. Es gibt hier keinen ESP-NOW-Master und keine MQTT-Bridge.

Die harte Regel: Topics und Payload-Felder bleiben kompatibel zur bisherigen Firmware, damit Node-RED nur den MQTT-Vertrag sieht.

## Import in Home Assistant

Kopiere den kompletten Ordner in die passende Home-Assistant-Konfiguration, damit die relativen `packages/`-Includes funktionieren.

Danach kannst du die Dateien aus `devices/` importieren oder als neue YAML anlegen. Die benoetigten Secret-Werte liegen in Home Assistant in der zentralen Secret-Datei:

```yaml
wifi_ssid: <dein-wlan-name>
wifi_password: <dein-wlan-passwort>
mqtt_broker: <broker-host-oder-ip>
mqtt_username: <mqtt-benutzer>
mqtt_password: <mqtt-passwort>
ota_password: <starkes-ota-passwort>
```

Wenn dein MQTT-Broker keine Zugangsdaten nutzt, muessen die `username`/`password`-Zeilen in den Device-YAMLs entfernt oder passend ersetzt werden.

Wichtig zur Ordnerstruktur: Die Device-YAMLs nutzen `!include ../packages/...`. Wenn die Geraetedateien unter `/config/esphome/devices/` liegen, muessen die Packages unter `/config/esphome/packages/` liegen. Wenn die gleichen Geraetedateien zusaetzlich direkt unter `/config/esphome/` liegen, wie es das ESPHome-Dashboard oft anzeigt, zeigen die Includes stattdessen auf `/config/packages/`. In diesem Fall muessen die `smarthome_*`-Packages und `setup_portal.h` auch nach `/config/packages/` kopiert werden.

Die ESPHome-YAMLs aktivieren OTA zentral ueber `packages/smarthome_contract_base.yaml`. Deshalb muss die zuerst per Kabel geflashte Factory-Firmware bereits mit demselben `ota_password` gebaut worden sein, sonst kann ein spaeteres Wireless-Update nicht sauber authentifizieren.

Fuer Diagnosezwecke ist in allen ESPHome-Geraeten `logger.level: DEBUG` aktiv. Zusaetzlich stellt `packages/smarthome_contract_base.yaml` die native ESPHome-API mit `reboot_timeout: 0s` und die `debug`-Komponente bereit, damit die Logs im ESPHome-Dashboard von Home Assistant per Netzwerk sichtbar sind. Der MQTT-Vertrag bleibt trotzdem die produktive Server-Schnittstelle; die API ist nur fuer ESPHome-Dashboard, Logs und Diagnose gedacht.

Wichtig bei manuellen Dateien:

- `firmware.factory.bin` ist nur fuer den ersten Flash per Kabel ab Adresse `0x0`.
- `firmware.ota.bin` ist fuer Wireless-Updates.

Eine Factory-Datei per OTA hochzuladen ist kein normales Update. Sie enthaelt Bootloader, Partitionstabelle und App-Image zusammen und ist fuer den Flash-Offset `0x0` gebaut. Fuer OTA muss das reine App-Image verwendet werden.

## Lokale Pruefung

Die YAMLs koennen lokal ohne echte Zugangsdaten geprueft werden. Das Skript legt dafuer eine Arbeitskopie mit Dummy-Secrets an und nutzt einen isolierten PlatformIO-Cache unter einem kurzen lokalen Pfad wie `D:\.sh-esphome\`. Der kurze Pfad ist unter Windows wichtig, weil ESPHome/PlatformIO sonst beim Compile an zu langen Befehlszeilen scheitern kann.

Nur Konfiguration validieren:

```powershell
.\scripts\check_esphome.ps1
```

Ein einzelnes Geraet validieren:

```powershell
.\scripts\check_esphome.ps1 -Device net_erl_hall_module_led_ring
```

Ein einzelnes Geraet kompilieren:

```powershell
.\scripts\check_esphome.ps1 -Device net_erl_hall_module_led_ring -Compile
```

Live-Abgleich gegen die Node-RED-Datenbank auf Home Assistant:

```powershell
.\scripts\check_ha_device_contract.ps1
```

Der Check kopiert nur die SQLite-Datenbank per SSH/SCP in ein temporaeres lokales Verzeichnis und vergleicht sie mit den Device-IDs aus `esphome/devices/`. Er meldet fehlende Geraete, unvollstaendige Metadaten, Offline-/Stale-Zustaende und alte oder falsche IDs, die noch im Server liegen.

Wenn PlatformIO-Tools lokal inkonsistent sind, kann der isolierte lokale Cache hart neu aufgebaut werden:

```powershell
.\scripts\check_esphome.ps1 -CleanPlatformioCache -Compile
```

Der erste Compile kann lange dauern, weil ESPHome und PlatformIO Abhaengigkeiten herunterladen. Echte WLAN- oder MQTT-Secrets gehoeren nicht ins Repo.

Die ESP32-C3-YAMLs pinnen bewusst `toolchain: platformio` und `framework.type: arduino`. ESPHome 2026.5.0 unterstuetzt zusaetzlich einen nativen ESP-IDF-Buildpfad, dieser ist fuer diese produktive MQTT-Linie aber nicht der Zielpfad.

## ESPHome-Toolchain-Reparatur

Wenn der Build in der ESPHome-Umgebung mit fehlendem `tool-cmake`, `idf_tools.py installation failed` oder unvollstaendigen `riscv32-esp-elf`-Tools scheitert, ist zuerst die Toolchain-Installation zu reparieren, nicht die Device-YAML.

Vor dem Loeschen Speicherplatz pruefen:

```sh
df -h
df -i
du -sh /data/cache/platformio 2>/dev/null
du -sh /root/.platformio 2>/dev/null
```

Gezielter Cleanup im Addon-Container:

```sh
rm -rf /data/cache/platformio/packages/tool-cmake
rm -rf /data/cache/platformio/packages/tool-ninja
rm -rf /data/cache/platformio/packages/toolchain-riscv32-esp
rm -rf /data/cache/platformio/packages/framework-espidf
rm -rf /data/cache/platformio/cache

rm -rf /root/.platformio/tools/tool-cmake
rm -rf /root/.platformio/tools/tool-ninja
rm -rf /root/.platformio/tools/riscv32-esp-elf-gdb
rm -rf /root/.platformio/tools/toolchain-riscv32-esp-elf
```

Danach die ESPHome-Umgebung neu starten, im Device Builder die Build-Dateien des betroffenen Geraets bereinigen und erneut kompilieren. Wenn der Fehler bleibt, den kompletten PlatformIO-Cache sichern/neu aufbauen:

```sh
mv /data/cache/platformio /data/cache/platformio.broken.$(date +%Y%m%d-%H%M%S)
```

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
