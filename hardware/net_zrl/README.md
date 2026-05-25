# NET-ZRL — 2-Relais-Knoten / Rolladen (Basetype)

> `device_class: net_zrl` | `SH_CLASS_NET_ZRL (0x02)`

## Übersicht

NET-ZRL ist der Basetype für **netzbetriebene 2-Relais-Knoten** mit Rolladen-/Cover-Steuerung. Sicherheitsschaltung durch serielle Relais-Verdrahtung.

## KiCAD-Quellen

| Platine | Pfad | Typ |
|---------|------|-----|
| 🔑 Modularer Steuerkreis | [../shared/modularer_steuerkreis/](../shared/modularer_steuerkreis/) | **Identisch mit net_erl!** |
| ⚡ Leistungskreis (230V) | [leistungskreis/](leistungskreis/) | NET-ZRL-spezifisch (2 Relais) |

### Leistungskreis (2-Relais)

👉 **[leistungskreis_2relais.md](leistungskreis_2relais.md)** — Vollständige Doku mit BOM

→ KiCAD: `Leistungskreis_korrektur.{kicad_pro,sch,pcb}` im Ordner [leistungskreis/](leistungskreis/)

→ SVG-Schaltplan: [schematics/leistungskreis.svg](schematics/leistungskreis.svg)

→ **Wichtig:** N-Kanal MOSFETs (IRLZ34N), 2× Optokoppler, Serien-Sicherheitsschaltung

### Steuerkreis

👉 **[../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md)** — **Derselbe** wie bei net_erl!

→ KiCAD: [../shared/modularer_steuerkreis/](../shared/modularer_steuerkreis/)

→ SVG: [../shared/schematics/steuerkreis.svg](../shared/schematics/steuerkreis.svg)

## Pinbelegung (Basetype)

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional |
| GPIO1 | I²C SCL | Optional |
| GPIO5 | Trigger_PIN2 | Relais 2 / Runter (via PC817) |
| GPIO10 | Trigger_PIN1 | Relais 1 / Hoch (via PC817) |
| GPIO20 | RX | UART Debug |
| GPIO21 | TX | UART Debug |

## Geräte (Devices)

| Gerät | ID | Besonderheiten |
|-------|-----|----------------|
| 👉 [NET-ZRL-002](devices/NET-ZRL-002_shutter_module/) | Rolladensteuerung | 2 Relais, 3 Taster, Kalibrierung |
