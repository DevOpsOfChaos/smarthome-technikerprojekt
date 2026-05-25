# Modularer Steuerkreis — KiCAD-Projekt

> Gemeinsame Steuerplatine für ALLE Relais-Knoten (`net_erl` und `net_zrl`)

## Dateien

| Datei | Beschreibung |
|-------|-------------|
| `steuerkreis.kicad_pro` | KiCAD 9.0 Projektdatei |
| `steuerkreis.kicad_sch` | Schaltplan (flat schematic) |
| `steuerkreis.kicad_pcb` | PCB-Layout (2-Lagen, 1.6mm) |
| `mod_Steuerkreis_ESP32.kicad_prl` | PCB-Regeln |
| `steuerkreis.csv` | Bauteilliste |
| `steuerkreis.step` | 3D-Modell (STEP-Export) |
| `Pinbelegung/Pinbelegung.txt` | GPIO-Belegung |
| `production/` | Fertigungsdaten (BOM, Position, Netlist) |

## Verwendung

Dieser Steuerkreis wird mit zwei verschiedenen Leistungskreisen kombiniert:

- **[net_erl](../../net_erl/leistungskreis/)** — 1-Relais-Leistungskreis
- **[net_zrl](../../net_zrl/leistungskreis/)** — 2-Relais-Leistungskreis

Die Verbindung erfolgt über ein 5-poliges JST-PH-Kabel.
