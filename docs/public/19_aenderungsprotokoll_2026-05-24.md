# Aenderungsprotokoll 2026-05-24

Dieses Protokoll dokumentiert die technischen Anpassungen am Server-, ESPHome- und Firmware-Pfad.

## MQTT-Broker und Node-RED

- Node-RED nutzt den MQTT-Broker nicht mehr hart codiert, sondern `MQTT_HOST` und `MQTT_PORT`.
- Der lokale Compose-Broker bleibt als Standard fuer Standalone-Betrieb und Tests erhalten.
- Fuer Installationen mit dauerhaftem Mosquitto im Netz muss `MQTT_HOST` auf diesen Broker zeigen.
- Die oeffentliche Dokumentation beschreibt nur den Netz-Broker und nennt keine internen Serverdetails.

## MQTT-Steuerbefehle

- Node-RED publiziert Steuerbefehle jetzt als JSON-Text, nicht als JavaScript-Objekt.
- Das betrifft manuelle Relaisbefehle, manuelle Rolladenbefehle und faellige Rolladen-Automatikbefehle.
- Der gemeinsame MQTT-Vertrag bleibt:
  - Topic `smarthome/device/<device_id>/command`
  - Feld `request_id`
  - Feld `command`
  - fuer Relais `relay_1`
  - fuer Rolladen `position` bei `set_position`

## ESPHome OTA

- OTA ist zentral im ESPHome-Basis-Package aktiviert.
- Die produktive Secret-Datei braucht dafuer `ota_password`.
- Die zuerst per Kabel geflashte Factory-Firmware muss bereits mit diesem OTA-Block gebaut sein.
- Batteriebetriebene ESPHome-Geraete markieren ihren kurzen Bootlauf als erfolgreich, damit Deep Sleep nicht als fehlgeschlagener Boot gewertet wird.

## ESP32-C3 Strapping-Pins

- Der Rolladen-Up-Taster wurde von `GPIO2` auf `GPIO20` gelegt.
- Hintergrund: `GPIO2` ist beim ESP32-C3 ein Strapping-Pin und kann beim Reset den Bootmodus beeinflussen.
- Geaendert wurden beide Codegruppen:
  - ESPHome: `devices/net_zrl_shutter_module.yaml`
  - eigene Firmware: `net_zrl_shutter_module` und der `net_zrl`-Fallback
- `GPIO0` und `GPIO1` bleiben fuer I2C reserviert.
- `GPIO8` bleibt beim LED-Ring-Modul als LED-Datenpin bestehen und sollte bei Hardwaretests weiter beobachtet werden.

## Geprueft

- Alle ESPHome-YAMLs validieren erfolgreich.
- Ein repraesentatives ESPHome-Geraet kompiliert erfolgreich mit OTA.
- Das erzeugte OTA-Image passt in den OTA-Slot.
- Node-RED-Flow-JSON ist syntaktisch gueltig.
- Die Node-RED-Function-Nodes fuer Steuerung und Rolladen-Automatik sind syntaktisch gueltig.
- Der MQTT-Befehlsvertrag passt zur ESPHome-Linie und zur Master-/Firmware-Linie.

## Offene Hardware-Pruefung

- Nach dem naechsten OTA-Test sollte die serielle Konsole beim ersten Neustart mitlaufen.
- Wenn weiterhin ein Bootloop auftritt, sind die ersten Bootmeldungen nach Reset entscheidend.
- Besonders zu pruefen sind Pegel auf ESP32-C3-Strapping-Pins waehrend Reset und Boot.
