# Master-Bridge — Hardware-Referenz

> ESP-NOW ↔ MQTT Bridge | `device_class: master` | `SH_CLASS_MASTER (0xFE)`

## Übersicht

Der Master ist die zentrale Bridge zwischen dem ESP-NOW-Mesh-Netzwerk und dem MQTT-Server (Node-RED auf Raspberry Pi). Er ist dauerhaft am Netzstrom und per WLAN mit dem Raspberry Pi verbunden.

## Hardware-Plattform

| Eigenschaft | Wert |
|-------------|------|
| Mikrocontroller | ESP32-C3 SuperMini Plus V2.0 |
| Versorgung | 5V DC (USB-C oder VIN-Pin) |
| Betriebsmodus | Dauerbetrieb (kein Sleep) |
| WLAN | 2.4 GHz, verbunden mit lokalem Netzwerk |
| ESP-NOW | Empfängt Statuspakete aller Nodes |
| MQTT | Publiziert Gerätestatus an Node-RED |

## Firmware

- **Verzeichnis:** `firmware/src/basetypes/master_firmware/`
- **main.cpp:** ~2700 Zeilen
- **Funktionen:**
  - ESP-NOW Peer-Management (dynamische Registry)
  - MQTT-Ingest-Pipeline
  - JSON-Serialisierung der Gerätezustände
  - Geräte-Hello/Heartbeat-Tracking
  - Diagnose-Endpunkte

## Geräte-ID

- **Primär:** `MASTER-001`

## Pin-Nutzung

Der Master nutzt das ESP32-C3 Board mit minimaler externer Beschaltung:

| GPIO | Funktion |
|------|----------|
| GPIO8 | WS2812 NeoPixel (Status-LED) |
| GPIO20 | UART RX (Debug) |
| GPIO21 | UART TX (Debug) |

Der Master hat **keine eigene Platine** — er verwendet das ESP32-C3 SuperMini Board direkt mit USB-C-Stromversorgung.

> Der Master ist das einzige Gerät ohne spezifische KiCAD-Platine. Alle anderen Geräteklassen haben dedizierte Platinen-Designs.
