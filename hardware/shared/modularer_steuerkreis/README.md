# Modularer Steuerkreis — KiCAD-Projekt

> Gemeinsame Steuerplatine für ALLE Relais-Knoten (`net_erl` und `net_zrl`)

## Dateien

| Datei | Beschreibung |
|-------|-------------|
| `mod_Steuerkreis_ESP32.kicad_pro` | KiCAD 9.0 Projektdatei |
| `mod_Steuerkreis_ESP32.kicad_sch` | Schaltplan (flat schematic) |
| `mod_Steuerkreis_ESP32.kicad_pcb` | PCB-Layout (2-Lagen, 1.6mm) |
| `mod_Steuerkreis_ESP32.kicad_prl` | PCB-Regeln |
| `mod_Steuerkreis_ESP32.csv` | Bauteilliste |
| `mod_Steuerkreis_ESP32.step` | 3D-Modell (STEP-Export) |
| `Pinbelegung/Pinbelegung.txt` | GPIO-Belegung |
| `production/` | Fertigungsdaten (BOM, Position, Netlist) |

## Verwendung

Dieser Steuerkreis wird mit zwei verschiedenen Leistungskreisen kombiniert:

- **[net_erl](../../net_erl/leistungskreis/)** — 1-Relais-Leistungskreis
- **[net_zrl](../../net_zrl/leistungskreis/)** — 2-Relais-Leistungskreis

Die Verbindung erfolgt über ein 5-poliges JST-PH-Kabel.
