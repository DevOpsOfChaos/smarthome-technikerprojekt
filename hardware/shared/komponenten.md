# Gemeinsame Komponenten-Referenz

> Datenblätter und technische Details der projektweit verwendeten Bauteile

## Zugehörige KiCAD-Quellen

| Komponente | KiCAD-Dateien |
|-----------|---------------|
| ESP32-C3 THT-Adapter | [esp32-c3-supermini/](esp32-c3-supermini/) |
| Modularer Steuerkreis | [modularer_steuerkreis/](modularer_steuerkreis/) |
| 1-Relais Leistungskreis | [../net_erl/leistungskreis/](../net_erl/leistungskreis/) |
| 2-Relais Leistungskreis | [../net_zrl/leistungskreis/](../net_zrl/leistungskreis/) |
| Sensor Netzbetrieb | [../net_sen/kicad/](../net_sen/kicad/) |
| Sensor Batteriebetrieb | [../bat_sen/kicad/](../bat_sen/kicad/) |

---

## Mikrocontroller & Adapter

### ESP32-C3 SuperMini Plus V2.0
- **Hersteller:** TENSTAR (Espressif ESP32-C3)
- **Datenblatt:** `esp32-c3_datasheet_en.pdf`, `esp32-c3_technical_reference_manual_en.pdf`
- Siehe [esp32-c3-supermini.md](esp32-c3-supermini.md) für Details

## Netzteile (ACDC-Wandler)

### HLK-5M05
- **Hersteller:** Hi-Link
- **Typ:** AC/DC-Wandler, 230V AC → 5V DC
- **Leistung:** 5W
- **Einsatz:** NET-ERL, NET-ZRL (Leistungskreis)
- **Datenblatt:** `1811141611_Hi-Link-HLK-5M05_C209904.pdf`

### HLK-PM03
- **Hersteller:** Hi-Link
- **Typ:** AC/DC-Wandler, 230V AC → 3.3V DC
- **Leistung:** 3W
- **Einsatz:** NET-SEN (Sensor_Netzbetrieb)
- **Datenblatt:** `2204251615_Hi-Link-HLK-PM03_C3029395.pdf`

## Relais

### Hongfa HF46F/5-HS1
- **Hersteller:** Hongfa
- **Typ:** Subminiatur-Leistungsrelais
- **Spulenspannung:** 5V DC
- **Kontaktbelastbarkeit:** 10A / 250V AC
- **Kontaktart:** 1× Schließer (1 Form A)
- **Abmessungen:** 20.5 × 10.2 × 15.3 mm
- **Einsatz:** NET-ERL (1×), NET-ZRL (2×)
- **Datenblatt:** `1811021711_Hongfa-HF46F-005-HS1T_C303944.pdf`

## Optokoppler

### PC817
- **Hersteller:** Sharp / diverse
- **Typ:** 1-Kanal Optokoppler, DIP-4 / SOP-4
- **Isolationsspannung:** 5000V RMS
- **Einsatz:** Galvanische Trennung ESP-GPIO → Relais-MOSFET
- **NET-ERL:** 1× (DIP-4)
- **NET-ZRL:** 2× (SOP-4 SMD)

## MOSFETs

### FQP27P06 (P-Kanal)
- **Typ:** P-Channel MOSFET, TO-220
- **Vds:** -60V, **Id:** -27A
- **Rds(on):** 70 mΩ
- **Einsatz:** NET-ERL Leistungskreis (Relais-Treiber)

### IRLZ34N (N-Kanal)
- **Typ:** N-Channel MOSFET, TO-220
- **Vds:** 55V, **Id:** 30A
- **Rds(on):** 35 mΩ
- **Einsatz:** NET-ZRL Leistungskreis (Relais-Treiber)

### IRF9530 (P-Kanal)
- **Typ:** P-Channel MOSFET, SOT-23 (SMD)
- **Einsatz:** BAT-SEN (Power-Gating für Deep-Sleep)

## Dioden

### 1N4007
- **Typ:** Standard-Gleichrichterdiode, 1000V/1A
- **Einsatz:** Freilaufdiode über Relaisspulen (Schutz vor induktiven Spannungsspitzen)
- **Gehäuse:** NET-ERL: DO-41 (bedrahtet), NET-ZRL: SOD-123 (SMD)

### BZX55C3V3
- **Typ:** Zener-Diode 3.3V, DO-35
- **Einsatz:** BAT-SEN (Überspannungsschutz)

## Sensoren

### BME280
- **Hersteller:** Bosch Sensortec
- **Typ:** Temperatur/Feuchte/Druck, I²C
- **Adresse:** 0x76 (SDO=GND) / 0x77 (SDO=VCC)
- **Einsatz:** NET-ERL-001, NET-SEN-002

### BME680
- **Hersteller:** Bosch Sensortec
- **Typ:** Temperatur/Feuchte/Druck/Gas (VOC), I²C
- **Adresse:** 0x76 (SDO=GND) / 0x77 (SDO=VCC)
- **Einsatz:** NET-ERL-002
- **Datenblatt:** `bst-bme680-ds001.pdf`

### VEML7700
- **Hersteller:** Vishay
- **Typ:** Lux-Sensor (0–120k Lux), I²C
- **Adresse:** 0x10
- **Einsatz:** NET-ERL-001/002, NET-SEN-002
- **Datenblatt:** `veml7700.pdf`

### ENS160
- **Hersteller:** ScioSense
- **Typ:** MOX-Gassensor (eCO₂, TVOC, AQI), I²C
- **Adresse:** 0x52 (SDO=GND) / 0x53 (SDO=VCC)
- **Einsatz:** NET-ERL-002
- **Datenblatt:** `ENS160-Datasheet.pdf`, `adafruit-ens160-mox-gas-sensor.pdf`

### AHT21
- **Hersteller:** ASAIR
- **Typ:** Temperatur/Feuchte, I²C
- **Adresse:** 0x38
- **Einsatz:** Optional auf allen I²C-fähigen Platinen
- **Datenblatt:** `Data Sheet AHT21.pdf`

### LD2410
- **Hersteller:** Hi-Link
- **Typ:** 24GHz Millimeterwellen-Radar (Präsenz/Bewegung)
- **Schnittstelle:** UART (GPIO6=RX, GPIO7=TX)
- **Einsatz:** NET-ERL-002
- **Reichweite:** 0.75–6m, konfigurierbar

### HC-SR501
- **Typ:** PIR-Bewegungssensor
- **Schnittstelle:** Digital (GPIO3)
- **Einsatz:** NET-ERL-001
- **Datenblatt:** `HC SR501 PIR Sensor Datasheet.pdf`

## Passive Bauteile (projektweit)

| Bauteil | Wert | Gehäuse | Einsatz |
|---------|------|---------|---------|
| R3, R4 | 4K7 | 0805 | I²C-Pullups (alle Platinen) |
| R1 | 330Ω | 0805 | MOSFET-Gate-Widerstand (NET-ZRL) |
| R2, R4 | 10K | 0805 | Gate-Pulldown (NET-ZRL) |
| R1 (BAT) | 10K | 0603 | Spannungsteiler (BAT-SEN) |
| R2, R5 (BAT) | 100K | 0603 | Spannungsteiler (BAT-SEN) |
| C1 | 100µF | Elko 6.3×5.8mm | Glättung (alle Netz-Platinen) |
| C2 | 100nF | 0805 | Entkopplung (alle Netz-Platinen) |
| C1 (BAT) | 100µF | 0603 | Glättung (BAT-SEN) |
| F1 | Feinsicherung | 5×20mm | 230V-Eingang |
