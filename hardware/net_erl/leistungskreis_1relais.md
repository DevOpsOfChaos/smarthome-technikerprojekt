# Leistungskreis — 1-Relais (NET-ERL)

> 230V-Seite für 1-Relais-Knoten | KiCAD: `Leistungskreis_1Relai_korrektur`

## Übersicht

Der Leistungskreis ist die **230V-führende** Platine des NET-ERL-Systems. Sie enthält das Netzteil, das Relais und die Ansteuerungselektronik mit galvanischer Trennung.

## KiCAD-Projekt

- **Projekt:** `Leistungskreis_1Relai_korrektur.kicad_pro`
- **Schematic:** `Leistungskreis_1Relai_korrektur.kicad_sch`
- **PCB:** `Leistungskreis_1Relai_korrektur.kicad_pcb`
- **Lagen:** 2 (F.Cu + B.Cu), 1.6 mm
- **3D-Export:** `Leistungskreis_1Relai_Steuerung.step`

## Stückliste (BOM)

| Designator | Bauteil | Footprint | Wert |
|-----------|---------|-----------|------|
| PS1 | Hi-Link HLK-5M05 | HLK-5Mxx | 230V AC → 5V DC |
| F1 | Feinsicherung | 5×20mm | — |
| K2 | Hongfa HF46F/5-HS1 | RELAY_HF46F | 1× Relais, 5V/10A |
| Q1 | FQP27P06 | TO-220 Vertikal | P-Kanal MOSFET |
| U2 | PC817 | DIP-4 | Optokoppler |
| D1 | 1N4007 | DO-41 | Freilaufdiode |
| C1 | 100µF | Elko 6.3×5.8mm | Glättung |
| C2 | 100nF | 0805 | Entkopplung |
| R1 | 330Ω | 0805 | Gate-Widerstand |
| R2 | 100K | 0805 | Gate-Pulldown |
| J1 | JST PH 5-pol | B5B-PH-K | Verbindung zum Steuerkreis |
| J5 | Phoenix MKDS 3-pol | TerminalBlock | — |
| J8 | Phoenix MKDS 3-pol | TerminalBlock | 230V-Eingang |

## Anschlüsse

| Klemme | Typ | Belegung |
|--------|-----|----------|
| J8 | Phoenix MKDS 1.5/3 | L, N, PE (230V AC Eingang) |
| J5 | Phoenix MKDS 1.5/3 | Schaltkontakt (Relais-Ausgang) |
| J1 | JST PH 5-pol | 5V, GND, GPIO10, GPIO5, 3V3 (zum Steuerkreis) |

## Schaltungsdesign

### Signalfluss

```
ESP GPIO10 ──► PC817 Optokoppler ──► FQP27P06 Gate ──► HF46F Spule ──► Relaiskontakt
                    │                      │                    │
              galvanische              P-Kanal              1N4007
              Trennung                 MOSFET            Freilaufdiode
```

### Funktionsweise

1. **GPIO10 = HIGH:** Optokoppler PC817 leitet → Gate des P-MOSFET wird auf GND gezogen → MOSFET schaltet durch → Relais-Spule bestromt → Kontakt schließt
2. **GPIO10 = LOW:** Optokoppler sperrt → Gate wird über R2 (100K) auf 5V gezogen → MOSFET sperrt → Relais fällt ab
3. **Freilaufdiode D1 (1N4007):** Schützt den MOSFET vor der induktiven Spannungsspitze beim Abschalten der Relaisspule

### Besonderheiten

- **P-Kanal MOSFET (FQP27P06):** High-Side-Schaltung — das Relais liegt zwischen MOSFET-Drain und GND
- **Optokoppler (PC817):** 5000V RMS Isolation zwischen ESP-Seite (3.3V Logik) und 230V-Seite
- **Feinsicherung F1:** Schutz vor Überlast auf der 230V-Seite
- **HLK-5M05:** Liefert 5V/1A — versorgt sowohl die Relaisspule (~100mA) als auch den ESP32-C3 (~80-300mA)

## Abmessungen

- Platinengröße: ca. 70 × 50 mm (2-Lagen, 1.6 mm FR4)
- Montage: Verschraubt im Gehäuse
