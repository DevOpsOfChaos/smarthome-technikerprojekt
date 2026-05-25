# Master-Bridge (Basetype)

> ESP-NOW ↔ MQTT Bridge | `device_class: master` | `SH_CLASS_MASTER (0xFE)`

## Übersicht

Der Master ist die zentrale Bridge zwischen dem ESP-NOW-Mesh-Netzwerk und dem MQTT-Server (Node-RED auf Raspberry Pi).

> **Kein eigenes PCB-Design.** Der Master verwendet das ESP32-C3 SuperMini Plus V2.0 direkt — ohne Trägerplatine.

## Hardware

| Eigenschaft | Wert |
|-------------|------|
| Board | TENSTAR ESP32-C3 SuperMini Plus V2.0 |
| Mikrocontroller | Espressif ESP32-C3 (RISC-V, 160 MHz) |
| Flash / SRAM | 4 MB / 400 KB |
| Versorgung | 5V DC via USB-C |
| Betriebsmodus | Dauerbetrieb (kein Sleep) |
| WLAN | 2.4 GHz, verbunden mit lokalem Netzwerk |
| MQTT-Broker | mosquitto auf Raspberry Pi |

## Aufbau

```
┌─────────────────────────────────┐
│  ESP32-C3 SuperMini Plus V2.0   │
│                                 │
│  ┌───────────────┐              │
│  │   USB-C       │── 5V Power   │
│  ├───────────────┤              │
│  │   ESP32-C3    │              │
│  │   RISC-V MCU  │              │
│  ├───────────────┤              │
│  │   PCB-Antenne │              │
│  │   + U.FL      │              │
│  ├───────────────┤              │
│  │   WS2812 LED  │── GPIO8      │
│  │   (Status)    │              │
│  └───────────────┘              │
│                                 │
│  Keine externe Beschaltung      │
│  Keine Trägerplatine            │
│  Keine Sensoren                 │
└─────────────────────────────────┘
```

## Pin-Nutzung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO8 | WS2812 RGB-LED | Statusanzeige (onboard) |
| GPIO20 | UART RX | Debug (intern) |
| GPIO21 | UART TX | Debug (intern) |

> Alle anderen GPIOs sind unbelegt. Der Master hat keine Sensoren, keine Relais, keine Taster.

## ESP-NOW ↔ MQTT Datenfluss

```
  BAT-SEN ──┐
  NET-SEN ──┼── ESP-NOW ──►  Master  ── MQTT ──►  Node-RED (RPi)
  NET-ERL ──┤               (dieses              │
  NET-ZRL ──┘                Gerät)              ├─ SQLite
                                                  └─ Dashboard
```

## Firmware

- **Verzeichnis:** `firmware/src/basetypes/master_firmware/`
- **main.cpp:** ~2700 Zeilen
- **Plattform:** PlatformIO, `espressif32`, Arduino-Framework

### Kernfunktionen
- ESP-NOW Peer-Management (dynamische Registry)
- MQTT-Ingest-Pipeline (smarthome/device/+/state)
- JSON-Serialisierung aller Gerätezustände
- Hello/Heartbeat-Tracking
- Diagnose-Endpunkte

## Geräte-ID

- **Primär:** `MASTER-001`
- **Sekundär (Fallback):** `MASTER-002`

## Unterschiede zu anderen Basetypes

| Eigenschaft | Master | net_erl / net_zrl | net_sen / bat_sen |
|-------------|--------|-------------------|-------------------|
| Eigenes PCB | ❌ | ✅ (modular) | ✅ (einteilig) |
| KiCAD-Projekt | ❌ | ✅ | ✅ |
| Sensoren | ❌ | optional (I²C) | ✅ |
| Aktoren | ❌ | ✅ (Relais) | ❌ |
| Sleep | ❌ | ❌ | bat_sen: ✅ |
| WLAN | ✅ (dauerhaft) | ❌ (nur ESP-NOW) | ❌ (nur ESP-NOW) |
| MQTT | ✅ | ❌ | ❌ |

## Plattform-Referenz

👉 [../shared/esp32-c3-supermini.md](../shared/esp32-c3-supermini.md) — Technische Daten des ESP32-C3 SuperMini Plus
