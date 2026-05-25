# Modularer Steuerkreis (`mod_Steuerkreis_ESP32`)

> Gemeinsame Steuerplatine für **alle** Relais-Knoten (`net_erl` und `net_zrl`)

## KiCAD-Quellen

Die vollständigen KiCAD-Projektdateien liegen unter:

👉 **[shared/modularer_steuerkreis/](modularer_steuerkreis/)** — `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, BOM, STEP, Fertigungsdaten

## Übersicht

Der modulare Steuerkreis ist die **identische** Trägerplatine für den ESP32-C3 in allen Relais-basierten Geräten. Er wird über ein 5-poliges JST-PH-Kabel mit dem jeweiligen Leistungskreis verbunden.

## Stückliste (BOM)

| Designator | Bauteil | Footprint | Anzahl | Wert |
|-----------|---------|-----------|--------|------|
| J1 | 5-Pin Verbindung zum Leistungskreis | SolderWire 0.25mm² | 1 | Wire_5 |
| J3 | GPIO-Breakout 1 | PinHeader 1×05 | 1 | Co_GPIOx1 |
| J6 | Versorgungs-Breakout | PinHeader 1×06 | 1 | Conn_Vx1 |
| J7 | GPIO-Breakout 3 | PinHeader 1×03 | 1 | Co_GPIOx3 |
| JP1, JP2 | I²C-Pullup Lötbrücken | SolderJumper-2 | 2 | Bridge_I2C |
| MH, MH_2 | Befestigungslöcher | MountingHole M3 | 2 | — |
| R3, R4 | I²C-Pullup-Widerstände | 0805 | 2 | 4K7 |
| TP1–TP5 | Testpunkte | TestPoint 1.0mm | 5 | — |
| U3 | ESP32-C3 Modul | MODULE_ESP32-C3_SUPERMINI_TH | 1 | ESP32-C3 |

## Pinbelegung (Steuerkreis → Leistungskreis)

Das 5-polige JST-PH-Kabel (J1) führt:

| Pin | Signal | Ziel im Leistungskreis |
|-----|--------|----------------------|
| 1 | 5V | Versorgung vom HLK-5M05 |
| 2 | GND | Masse |
| 3 | GPIO10 (Trigger_PIN1) | Relais-1 Optokoppler |
| 4 | GPIO5 (Trigger_PIN2) | Relais-2 Optokoppler (nur net_zrl) |
| 5 | 3V3 | Optionale 3.3V-Referenz |

## Pinbelegung (GPIO-Breakouts)

| Pin | J3 (Co_GPIOx1) | J7 (Co_GPIOx3) | J6 (Conn_Vx1) |
|-----|---------------|---------------|---------------|
| 1 | GPIO0 (I²C SDA) | GPIO6 | 5V |
| 2 | GPIO1 (I²C SCL) | GPIO7 | 3V3 |
| 3 | GPIO2 | GPIO8 | GND |
| 4 | GPIO3 | — | GPIO4 |
| 5 | GPIO4 | — | GPIO9 |
| 6 | — | — | GPIO10 |

## SVG-Schaltplan

👉 **[schematics/steuerkreis.svg](schematics/steuerkreis.svg)**

## Kompatibilität

| Eigenschaft | net_erl | net_zrl |
|-------------|---------|---------|
| Steuerkreis-Platine | ✅ Identisch | ✅ Identisch |
| KiCAD-Quellen | [shared/modularer_steuerkreis/](modularer_steuerkreis/) | ← dieselben |
| Versorgung (5V vom Leistungskreis) | ✅ HLK-5M05 | ✅ HLK-5M05 |
| Trigger_PIN1 (GPIO10) | ✅ Relais 1 | ✅ Relais 1 (Hoch) |
| Trigger_PIN2 (GPIO5) | NC | ✅ Relais 2 (Runter) |

## Designmerkmale

- **Galvanische Trennung:** Der Steuerkreis hat KEINE direkte Verbindung zum 230V-Netz. Die Isolation erfolgt über die Optokoppler (PC817) im Leistungskreis.
- **Testpunkte:** TP1–TP5 ermöglichen Debugging ohne die Platine zu modifizieren
- **M3-Montage:** Zwei Befestigungslöcher für Gehäuseeinbau
