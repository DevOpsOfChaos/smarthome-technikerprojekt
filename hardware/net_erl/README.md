# NET-ERL — 1-Relais-Knoten (Basetype)

> `device_class: net_erl` | `SH_CLASS_NET_ERL (0x01)`

## Übersicht

NET-ERL ist der Basetype für **netzbetriebene 1-Relais-Knoten**. Einsatz: Lampen, Steckdosen, Flurlicht-Steuerung.

## KiCAD-Quellen

| Platine | Pfad | Typ |
|---------|------|-----|
| 🔑 Modularer Steuerkreis | [../shared/modularer_steuerkreis/](../shared/modularer_steuerkreis/) | Gemeinsam mit net_zrl |
| ⚡ Leistungskreis (230V) | [leistungskreis/](leistungskreis/) | NET-ERL-spezifisch |
| 🔬 Simulation | [simulation/](simulation/) | LTSpice |

### Leistungskreis

👉 **[leistungskreis_1relais.md](leistungskreis_1relais.md)** — Vollständige Doku mit BOM und Schaltungsdesign

→ KiCAD: `Leistungskreis_1Relai_korrektur.{kicad_pro,sch,pcb}` im Ordner [leistungskreis/](leistungskreis/)

→ SVG-Schaltplan: [schematics/Leistungskreis_1Relai_korrektur.svg](schematics/Leistungskreis_1Relai_korrektur.svg)

### Steuerkreis

Der Steuerkreis ist **identisch** mit dem von net_zrl:

👉 **[../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md)** — Vollständige Doku

→ KiCAD: [../shared/modularer_steuerkreis/](../shared/modularer_steuerkreis/)

→ SVG: [../shared/schematics/mod_Steuerkreis_ESP32.svg](../shared/schematics/mod_Steuerkreis_ESP32.svg)

## Pinbelegung (Basetype)

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Sensoren (optional) |
| GPIO1 | I²C SCL | Sensoren (optional) |
| GPIO5 | Trigger_PIN2 | NC bei 1-Relais |
| GPIO10 | Trigger_PIN1 | Relais (via PC817) |
| GPIO20 | RX | UART Debug |
| GPIO21 | TX | UART Debug |

## Geräte (Devices)

| Gerät | ID | Sensoren |
|-------|-----|----------|
| 👉 [NET-ERL-001](devices/NET-ERL-001_hall_module/) | Flurmodul Basis | BME280, VEML7700, PIR |
| 👉 [NET-ERL-002](devices/NET-ERL-002_hall_module_led_ring/) | Flurmodul LED-Ring | BME680, VEML7700, ENS160, LD2410, NeoPixel |

→ Jedes Gerät hat eigene GPIO-Belegung und Sensor-Konfiguration. Siehe jeweiligen Device-Ordner.
