# NET-SEN — Netzbetriebener Sensor-Knoten (Basetype)

> `device_class: net_sen` | `SH_CLASS_NET_SEN (0x03)`

## Übersicht

NET-SEN ist der Basetype für **dauerhaft netzbetriebene Sensor-Knoten**. Kein Sleep — immer online.

## KiCAD-Quellen

👉 **[kicad/](kicad/)** — Vollständiges KiCAD-Projekt

| Datei | Beschreibung |
|-------|-------------|
| `net_sen.kicad_pro` | Projektdatei |
| `net_sen.kicad_sch` | Root-Schaltplan (hierarchisch) |
| `Leistungskreis.kicad_sch` | Sub-Sheet: 230V-Netzteil |
| `Steuerstromkreis.kicad_sch` | Sub-Sheet: ESP32 + I²C |
| `net_sen.kicad_pcb` | PCB-Layout (2-Lagen) |
| `Bilder/` | Layout-Ansichten (7 PNGs) |
| `production/` | BOM, Position, Netlist |

### SVG-Schaltpläne

- [schematics/net_sen.svg](schematics/net_sen.svg) — Root
- [schematics/net_sen-Leistungskreis.svg](schematics/net_sen-Leistungskreis.svg) — Netzteil
- [schematics/net_sen-Steuerstromkreis.svg](schematics/net_sen-Steuerstromkreis.svg) — ESP32

## Platinen-Design

Einteiliger Aufbau (nicht modular):

- **HLK-PM03:** 230V AC → 3.3V DC
- **Feinsicherung:** 5×20mm
- **I²C:** GPIO0/1 mit 4K7 Pullups (Lötbrücken)
- **230V-Eingang:** Phoenix MKDS 1.5/3

## Pinbelegung (Basetype)

| GPIO | Funktion |
|------|----------|
| GPIO0 | I²C SDA |
| GPIO1 | I²C SCL |
| GPIO2–10 | frei (Pinheader) |
| GPIO20/21 | UART |

## Versionen

1. **Standard Version** (aktiv) — im Ordner [kicad/](kicad/)
2. **Version mit TCA9548APWR** — Mit I²C-Multiplexer (separater KiCAD-Ordner)

## Geräte (Devices)

| Gerät | ID | Sensoren |
|-------|-----|----------|
| 👉 [NET-SEN-002](devices/NET-SEN-002_weather_station/) | Wetterstation | BME280, VEML7700, Regensensor |
