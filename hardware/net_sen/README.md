# NET-SEN — Netzbetriebener Sensor-Knoten (Hardware-Referenz)

> `device_class: net_sen` | `SH_CLASS_NET_SEN (0x03)` | KiCAD: `Sensor_Netzbetrieb`

## Übersicht

NET-SEN ist die Geräteklasse für **dauerhaft netzbetriebene Sensor-Knoten**. Im Gegensatz zu BAT-SEN schlafen diese Geräte nie — sie sind immer online und melden kontinuierlich.

## Platinen-Design

NET-SEN hat einen **einteiligen** Platinenaufbau (nicht modular wie die Relais-Knoten).

```
┌─────────────────────────────────────┐
│        Sensor_Netzbetrieb           │
│                                     │
│  HLK-PM03 (230V→3.3V)              │
│  ESP32-C3 SuperMini (THT-Adapter)  │
│  I²C-Pullups (4K7, Lötbrücken)     │
│  GPIO-Pinheader für Sensoren       │
│  Feinsicherung (5×20mm)            │
│  230V-Eingang (Phoenix-Klemme)     │
└─────────────────────────────────────┘
```

### KiCAD-Projekt

- **Standard:** `Sensor_Netzbetrieb/Standard Version/Sensor_all_Netz_ESP32.kicad_pro`
- **TCA-Variante:** `Sensor_Netzbetrieb/Version mit TCA9548APWR/Sensor_all_Netz_ESP32.kicad_pro`
- **Lagen:** 2 (F.Cu + B.Cu), 1.6 mm

### Stückliste (Standard Version)

| Designator | Bauteil | Footprint | Wert |
|-----------|---------|-----------|------|
| PS1 | Hi-Link HLK-PM03 | HLK-PMxx | 230V AC → 3.3V DC |
| F1 | Feinsicherung | 5×20mm | — |
| C1 | 100µF | Elko 6.3×5.8mm | Glättung |
| C2 | 100nF | 0805 | Entkopplung |
| ESP32 | ESP32-C3 SuperMini | MODULE_ESP32-C3_SUPERMINI_TH | — |
| J1 | Co_x6_GND | PinHeader 1×06 | GND-Breakout |
| J2 | Co_x6_3V3 | PinHeader 1×06 | 3.3V-Breakout |
| J3 | Co_GPIOx1 | PinHeader 1×05 | I²C-Breakout |
| J4 | Co_GPIOx2 | PinHeader 1×05 | GPIO-Breakout |
| J5 | Input_x3 | Phoenix MKDS 1.5/3 | 230V-Eingang (L, N, PE) |
| J7 | Co_GPIOx3 | PinHeader 1×03 | GPIO-Breakout |
| JP1, JP2 | Bridge_I2C | SolderJumper-2 | I²C-Pullup-Brücken |
| R3, R4 | 4K7 | 0805 | I²C-Pullups |

### Versionen

1. **Standard Version:** Basisplatine mit I²C-Breakout
2. **Version mit TCA9548APWR:** Mit I²C-Multiplexer für mehrere gleichartige Sensoren an einem Bus

## Pinbelegung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Sensoren |
| GPIO1 | I²C SCL | Sensoren |
| GPIO2–10 | frei | Verfügbar über Pinheader |

## Geräte dieser Klasse

| Gerät | ID | Sensoren |
|-------|-----|----------|
| [NET-SEN-002](NET-SEN-002_weather_station.md) | Wetterstation | BME280, VEML7700, digitaler Regensensor |
