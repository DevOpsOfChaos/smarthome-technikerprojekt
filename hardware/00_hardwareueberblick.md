# Hardware-Übersicht

> Stand: Mai 2026 | Plattform: ESP32-C3 (RISC-V) | Entwicklung: KiCAD 9.0

## Architektur

Alle Geräte basieren auf dem **TENSTAR ESP32-C3 SuperMini Plus V2.0** (ESP32-C3, RISC-V 32-Bit, 160 MHz, 4 MB Flash, 400 KB SRAM, WLAN b/g/n, BLE 5.0). Der Mikrocontroller wird über eineThrough-Hole-Adapterplatine (`ESP32-C3_SUPERMINI_TH`) auf die Trägerplatinen gesteckt.

Die Kommunikation erfolgt über **ESP-NOW** (Node → Master) und **MQTT** (Master → Server).

## Geräteklassen

| Klasse | Typ | Platinen-Design | Versorgung | Anzahl |
|--------|-----|----------------|------------|--------|
| `net_erl` | 1-Relais-Knoten | Relais_1fach (modular) | 230V (HLK-5M05) | 2 Geräte |
| `net_zrl` | 2-Relais-Knoten (Rolladen) | Relais_2fach (modular) | 230V (HLK-5M05) | 1 Gerät |
| `net_sen` | Netz-Sensor | Sensor_Netzbetrieb | 230V (HLK-PM03) | 1 Gerät |
| `bat_sen` | Batterie-Sensor | Sensor_Batteriebetrieb | Akku/Batterie | 2 Geräte |
| `master` | ESP-NOW↔MQTT Bridge | — | 230V | 1 Gerät |

## Modularer Platinenaufbau (Relais-Knoten)

Alle Relais-Knoten (`net_erl`, `net_zrl`) verwenden einen **zweiteiligen, modularen Aufbau**:

1. **Modularer Steuerkreis** (`mod_Steuerkreis_ESP32`) — **identisch für alle Relais-Varianten**
   - ESP32-C3 SuperMini auf THT-Adapter
   - I²C-Pullups (4K7, über Lötbrücken schaltbar)
   - GPIO-Pinheader für Sensoren
   - 5 Testpunkte für Debugging
   - 2× M3-Befestigungslöcher

2. **Leistungskreis** — **variiert je nach Relais-Typ**
   - `net_erl`: 1× Relais (HF46F), 1× P-Kanal MOSFET (FQP27P06), 1× Optokoppler (PC817)
   - `net_zrl`: 2× Relais (HF46F), 2× N-Kanal MOSFET (IRLZ34N), 2× Optokoppler (PC817)
   - Beide: Hi-Link ACDC-Wandler (230V→5V), Feinsicherung (5×20mm), 230V-Klemmen

Die beiden Platinen werden über ein 5-poliges JST-PH-Kabel verbunden (5V, GND, GPIO10, GPIO5, 3V3).

## Pinbelegungs-Matrix (alle Geräteklassen)

| GPIO | BAT-SEN | NET-SEN | NET-ERL | NET-ZRL | Funktion |
|------|---------|---------|---------|---------|----------|
| 0 | I²C SDA | I²C SDA | I²C SDA | I²C SDA | I²C-Daten |
| 1 | I²C SCL | I²C SCL | I²C SCL | I²C SCL | I²C-Takt |
| 2 | V-Mess (Batterie) | frei | NC | NC | Batterie-ADC |
| 3 | frei | frei | NC | NC | — |
| 4 | frei | frei | NC | NC | — |
| 5 | frei | frei | Trigger_PIN2 | Trigger_PIN2 (Runter) | Relais 2 |
| 6 | frei | frei | frei | frei | — |
| 7 | frei | frei | frei | frei | — |
| 8 | frei | frei | frei | frei | (Onboard-LED) |
| 9 | frei | frei | NC | NC | — |
| 10 | frei | frei | Trigger_PIN1 | Trigger_PIN1 (Hoch) | Relais 1 |
| 20 | RX | RX | RX | RX | UART RX |
| 21 | TX | TX | TX | TX | UART TX |

## Stromversorgungs-Designs

| Typ | Eingang | Wandler | Ausgang | Besonderheit |
|-----|---------|---------|---------|-------------|
| BAT-SEN | Akku (3.0–4.2V) | — (P-MOSFET Gate, Zener-Schutz) | 3.3V (LDO on ESP) | Deep-Sleep optimiert |
| NET-SEN | 230V AC | HLK-PM03 | 3.3V DC | Feinsicherung |
| NET-ERL (Leistung) | 230V AC | HLK-5M05 | 5V DC | Versorgt Relais-Spule + ESP |
| NET-ZRL (Leistung) | 230V AC | HLK-5M05 | 5V DC | Versorgt 2× Relais-Spule + ESP |

## Zentrale Designmuster

1. **I²C-Lötbrücken (JP1/JP2):** Auf allen Platinen können die 4K7-Pullups für I²C per Lötbrücke ein-/ausgeschaltet werden
2. **Optokoppler-Isolation (PC817):** Galvanische Trennung zwischen ESP-GPIO und Relais-MOSFET-Treibern
3. **Freilaufdioden (1N4007):** Schutz vor induktiven Spannungsspitzen der Relaisspulen
4. **Testpunkte (TP1-TP5):** Auf dem modularen Steuerkreis für Debugging
5. **Sensor-Pinheader:** Alle Platinen brechen GPIOs, GND und VCC auf Stiftleisten für externe Sensoren aus

## Verzeichnisstruktur

```
hardware/
├── 00_hardwareueberblick.md          ← diese Datei
├── shared/                           ← Gemeinsame Designs
│   ├── esp32-c3-supermini.md
│   ├── modularer_steuerkreis.md
│   └── komponenten.md
├── master/                           ← Master-Bridge
├── net_erl/                          ← 1-Relais-Knoten
├── net_zrl/                          ← 2-Relais-Knoten (Rolladen)
├── net_sen/                          ← Netz-Sensoren
└── bat_sen/                          ← Batterie-Sensoren
```

## Verwendete Komponenten (Übersicht)

| Komponente | Typ | Einsatz |
|-----------|-----|---------|
| ESP32-C3 SuperMini Plus V2.0 | Mikrocontroller | Alle Geräte |
| ESP32-C3_SUPERMINI_TH | THT-Adapter | Alle Geräte |
| HLK-5M05 | ACDC-Wandler 230V→5V | NET-ERL, NET-ZRL |
| HLK-PM03 | ACDC-Wandler 230V→3.3V | NET-SEN |
| Hongfa HF46F/5-HS1 | Relais 5V/10A | NET-ERL (1×), NET-ZRL (2×) |
| PC817 | Optokoppler | NET-ERL (1×), NET-ZRL (2×) |
| FQP27P06 | P-Kanal MOSFET TO-220 | NET-ERL |
| IRLZ34N | N-Kanal MOSFET TO-220 | NET-ZRL |
| 1N4007 | Freilaufdiode | Alle Relais-Platinen |
| BME280 | Temp/Feuchte/Druck (I²C 0x76) | NET-ERL-001, NET-SEN-002 |
| BME680 | Temp/Feuchte/Druck/Gas (I²C 0x76) | NET-ERL-002 |
| VEML7700 | Lux-Sensor (I²C 0x10) | NET-ERL-001/002, NET-SEN-002 |
| ENS160 | MOX-Gas-Sensor (I²C 0x52) | NET-ERL-002 |
| LD2410 | Radar-Präsenz (UART) | NET-ERL-002 |
| NeoPixel Ring (12 LED) | WS2812 | NET-ERL-002 |
| BZX55C3V3 | Zener-Diode 3.3V | BAT-SEN |
| IRF9530 | P-Kanal MOSFET SOT-23 | BAT-SEN |
