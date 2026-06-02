# BAT-SEN — Batteriebetriebener Sensor-Knoten (Basetype)

> `device_class: bat_sen` | `SH_CLASS_BAT_SEN (0x04)`

## Übersicht

BAT-SEN ist der Basetype für **batteriebetriebene Sensor-Knoten** mit Deep-Sleep. Optimiert für minimale Ruhestromaufnahme.

## KiCAD-Quellen

👉 **[kicad/](kicad/)** — Vollständiges KiCAD-Projekt

| Datei | Beschreibung |
|-------|-------------|
| `bat_sen.kicad_pro` | Projektdatei |
| `bat_sen.kicad_sch` | Root-Schaltplan (hierarchisch) |
| `Leistungskreis.kicad_sch` | Sub-Sheet: Power-Gating, Batterie |
| `Steuerstromkreis.kicad_sch` | Sub-Sheet: ESP32 + I²C |
| `bat_sen.kicad_pcb` | PCB-Layout (2-Lagen) |
| `Pinbelegung/Pinbelegung.txt` | GPIO-Belegung |
| `Code/SamplecodeBasis.txt` | ESP-NOW Deep-Sleep Beispiel |
| `production/` | BOM, Position, Netlist |

### SVG-Schaltpläne

- [schematics/bat_sen.svg](schematics/bat_sen.svg) — Root
- [schematics/bat_sen-Leistungskreis.svg](schematics/bat_sen-Leistungskreis.svg) — Power-Gating
- [schematics/bat_sen-Steuerstromkreis.svg](schematics/bat_sen-Steuerstromkreis.svg) — ESP32

## Stromspar-Design

- **Power-Gating:** P-MOSFET (IRF9530, SOT-23) schaltet Sensoren im Deep-Sleep ab
- **Zener-Diode:** BZX55C3V3 (DO-35) — Überspannungsschutz
- **Spannungsteiler:** R2+R5 (100K+100K) an GPIO4 — Batterieüberwachung
- **Deep-Sleep:** ~0.5–0.8 mA Board-Gesamtstrom
- **Wake-Up:** Timer (RTC) oder externer Interrupt

## Pinbelegung (Basetype)

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional |
| GPIO1 | I²C SCL | Optional |
| GPIO2 | Setup-Button | Lokaler Setup-/Bring-up-Taster |
| GPIO4 | V-Mess (ADC) | Batteriespannung via Spannungsteiler |
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
| 👉 [bat_sen_01](devices/bat_sen_01_window_contact/) | Fensterkontakt | Reed-Schalter | 2x AAA in Serie |
| 👉 [bat_sen_02](devices/bat_sen_02_rain_sensor/) | Regensensor | ADC-Regensensor | 2× AA |
