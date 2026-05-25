# Master-Bridge (Basetype)

> ESP-NOW ↔ MQTT Bridge | `device_class: master` | `SH_CLASS_MASTER (0xFE)`

## Übersicht

Der Master ist die zentrale Bridge zwischen ESP-NOW-Mesh und MQTT-Server (Node-RED).

## Hardware

| Eigenschaft | Wert |
|-------------|------|
| Mikrocontroller | ESP32-C3 SuperMini Plus V2.0 |
| Versorgung | 5V DC (USB-C) |
| Betrieb | Dauerbetrieb, kein Sleep |
| Eigene Platine | **Keine** — Bare-Metal-ESP32 |

> Der Master hat kein eigenes KiCAD-Projekt. Er verwendet das ESP32-C3 Board direkt.

## Pin-Nutzung

| GPIO | Funktion |
|------|----------|
| GPIO8 | WS2812 Status-LED |
| GPIO20/21 | UART Debug |

## Firmware

- **Verzeichnis:** `firmware/src/basetypes/master_firmware/`
- **main.cpp:** ~2700 Zeilen
- **Funktionen:** ESP-NOW Peer-Management, MQTT-Ingest, JSON-Serialisierung
- **Geräte-ID:** `MASTER-001`
