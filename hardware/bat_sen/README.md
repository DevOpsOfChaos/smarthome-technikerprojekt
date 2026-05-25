# BAT-SEN — Batteriebetriebener Sensor-Knoten (Basetype)

> `device_class: bat_sen` | `SH_CLASS_BAT_SEN (0x04)`

## Übersicht

BAT-SEN ist der Basetype für **batteriebetriebene Sensor-Knoten** mit Deep-Sleep. Optimiert für minimale Ruhestromaufnahme.

## KiCAD-Quellen

👉 **[kicad/](kicad/)** — Vollständiges KiCAD-Projekt

| Datei | Beschreibung |
|-------|-------------|
| `Sensor_all_ESP32.kicad_pro` | Projektdatei |
| `Sensor_all_ESP32.kicad_sch` | Root-Schaltplan (hierarchisch) |
| `Leistungskreis.kicad_sch` | Sub-Sheet: Power-Gating, Batterie |
| `Steuerstromkreis.kicad_sch` | Sub-Sheet: ESP32 + I²C |
| `Sensor_all_ESP32.kicad_pcb` | PCB-Layout (2-Lagen) |
| `Pinbelegung/Pinbelegung.txt` | GPIO-Belegung |
| `Code/SamplecodeBasis.txt` | ESP-NOW Deep-Sleep Beispiel |
| `production/` | BOM, Position, Netlist |

### SVG-Schaltpläne

- [schematics/Sensor_all_ESP32.svg](schematics/Sensor_all_ESP32.svg) — Root
- [schematics/Sensor_all_ESP32-Leistungskreis.svg](schematics/Sensor_all_ESP32-Leistungskreis.svg) — Power-Gating
- [schematics/Sensor_all_ESP32-Steuerstromkreis.svg](schematics/Sensor_all_ESP32-Steuerstromkreis.svg) — ESP32

## Stromspar-Design

- **Power-Gating:** P-MOSFET (IRF9530, SOT-23) schaltet Sensoren im Deep-Sleep ab
- **Zener-Diode:** BZX55C3V3 (DO-35) — Überspannungsschutz
- **Spannungsteiler:** R2+R5 (100K+100K) an GPIO2 — Batterieüberwachung
- **Deep-Sleep:** ~0.5–0.8 mA Board-Gesamtstrom
- **Wake-Up:** Timer (RTC) oder externer Interrupt

## Pinbelegung (Basetype)

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional |
| GPIO1 | I²C SCL | Optional |
| GPIO2 | V-Mess (ADC) | Batteriespannung via Spannungsteiler |
| GPIO3–10 | frei | Pinheader |
| GPIO20/21 | UART | Debug |

## Batterie-Profile

| Profil | Typ | Spannung | Kapazität |
|--------|-----|----------|-----------|
| `BAT_PROFILE_CR2032` | CR2032 | 3.0V | ~225 mAh |
| `BAT_PROFILE_2X_AA` | 2× AA | 3.0V | ~2500 mAh |
| `BAT_PROFILE_2X_AAA` | 2× AAA | 3.0V | ~1000 mAh |

## Geräte (Devices)

| Gerät | ID | Sensoren | Batterie |
|-------|-----|----------|----------|
| 👉 [bat_sen_01](devices/bat_sen_01_window_contact/) | Fensterkontakt | Reed-Schalter | CR2032 |
| 👉 [bat_sen_02](devices/bat_sen_02_rain_sensor/) | Regensensor | ADC-Regensensor | 2× AA |
