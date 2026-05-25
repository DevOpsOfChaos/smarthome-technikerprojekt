# Leistungskreis — 2-Relais (NET-ZRL)

> 230V-Seite für 2-Relais-Knoten | KiCAD: `Leistungskreis_korrektur`

## Übersicht

Der Leistungskreis ist die 230V-führende Platine des NET-ZRL-Systems. Sie enthält das Netzteil, zwei Relais in Sicherheitsschaltung und die Ansteuerungselektronik.

## KiCAD-Projekt

- **Projekt:** `leistungskreis.kicad_pro`
- **Schematic:** `leistungskreis.kicad_sch`
- **PCB:** `leistungskreis.kicad_pcb`
- **Lagen:** 2 (F.Cu + B.Cu), 1.6 mm

## Stückliste (BOM)

| Designator | Bauteil | Footprint | Wert |
|-----------|---------|-----------|------|
| PS1 | Hi-Link HLK-5M05 | HLK-5Mxx | 230V AC → 5V DC |
| F1 | Feinsicherung | 5×20mm | — |
| K1, K2 | Hongfa HF46F/5-HS1 | RELAY_HF46F | 2× Relais, 5V/10A |
| Q1, Q2 | IRLZ34N | TO-220 Vertikal | N-Kanal MOSFET |
| U1, U2 | PC817 | SOP-4 (SMD) | 2× Optokoppler |
| D1, D2 | 1N4007 | SOD-123 (SMD) | 2× Freilaufdioden |
| C1 | 100µF | Elko 6.3×5.8mm | Glättung |
| C2 | 100nF | 0805 | Entkopplung |
| R1, R3 | 330Ω | 0805 | Gate-Widerstände |
| R2, R4 | 10K | 0805 | Gate-Pulldowns |
| J3 | JST PH 5-pol | B5B-PH-K | Verbindung zum Steuerkreis |
| J8 | Phoenix MKDS 3-pol | TerminalBlock | 230V-Eingang (L, N, PE) |
| J1 | Phoenix MKDS 2-pol | TerminalBlock | Motor-Ausgang |
| J5 | Phoenix MKDS 2-pol | TerminalBlock | PE-Anschluss |

## Anschlüsse

| Klemme | Typ | Belegung |
|--------|-----|----------|
| J8 | Phoenix MKDS 1.5/3 | L, N (230V AC Eingang) |
| J5 | Phoenix MKDS 1.5/2 | PE (Schutzerde) |
| J1 | Phoenix MKDS 1.5/2 | Motor-Ausgang (geschaltet) |
| J3 | JST PH 5-pol | 5V, GND, GPIO10, GPIO5, 3V3 (zum Steuerkreis) |

## Schaltungsdesign

### Signalfluss

```
ESP GPIO10 ──► PC817 (U1) ──► IRLZ34N (Q1) ──► HF46F (K1) ──► Motor Hoch
                    │              │                  │
              galvanische       N-Kanal           1N4007 (D1)
              Trennung          MOSFET         Freilaufdiode

ESP GPIO5  ──► PC817 (U2) ──► IRLZ34N (Q2) ──► HF46F (K2) ──► Motor Runter
                    │              │                  │
              galvanische       N-Kanal           1N4007 (D2)
              Trennung          MOSFET         Freilaufdiode
```

### Funktionsweise

1. **GPIO10 = HIGH:** U1 leitet → Q1-Gate auf 5V → Q1 schaltet durch → K1-Spule bestromt → Motor fährt HOCH
2. **GPIO5 = HIGH:** U2 leitet → Q2-Gate auf 5V → Q2 schaltet durch → K2-Spule bestromt → Motor fährt RUNTER
3. **Beide GPIOs = LOW:** Beide MOSFETs sperren → Motor steht
4. **Sicherheit:** Relaiskontakte in Serie — unmöglich, dass beide gleichzeitig schließen

### Unterschiede zum 1-Relais-Leistungskreis

| Eigenschaft | NET-ERL (1-Relais) | NET-ZRL (2-Relais) |
|-------------|-------------------|---------------------|
| Relais-Anzahl | 1× HF46F | 2× HF46F |
| MOSFET-Typ | FQP27P06 (P-Kanal) | IRLZ34N (N-Kanal) |
| MOSFET-Anzahl | 1× | 2× |
| Optokoppler | 1× PC817 (DIP-4) | 2× PC817 (SOP-4 SMD) |
| Freilaufdioden | 1× 1N4007 (DO-41) | 2× 1N4007 (SOD-123) |
| Gate-Widerstand | 330Ω + 100K Pulldown | 2× 330Ω + 2× 10K Pulldown |
| Relais-Verdrahtung | Einfach (Schließer) | Serie (Sicherheit) |
| Ausgang | 1× Schaltkontakt | 1× Motor (Hoch/Runter) |

## Abmessungen

- Platinengröße: ca. 80 × 55 mm (2-Lagen, 1.6 mm FR4)
- Montage: Verschraubt im Gehäuse
