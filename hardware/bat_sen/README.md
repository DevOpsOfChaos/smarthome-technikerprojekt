# BAT-SEN — Batteriebetriebener Sensor-Knoten (Hardware-Referenz)

> `device_class: bat_sen` | `SH_CLASS_BAT_SEN (0x04)` | KiCAD: `Sensor_Batteriebetrieb`

## Übersicht

BAT-SEN ist die Geräteklasse für **batteriebetriebene Sensor-Knoten** mit Deep-Sleep-Betrieb. Optimiert für minimalen Ruhestrom und lange Batterielaufzeiten.

## Platinen-Design

BAT-SEN hat einen **einteiligen** Platinenaufbau ohne Netzteil:

```
┌─────────────────────────────────────┐
│      Sensor_Batteriebetrieb         │
│                                     │
│  Kein Netzteil (externe Batterie)   │
│  P-MOSFET Power-Gating (IRF9530)    │
│  Zener-Überspannungsschutz (BZX55C3V3)│
│  Spannungsteiler für Batterie-ADC   │
│  ESP32-C3 SuperMini (THT-Adapter)   │
│  I²C-Pullups (4K7, Lötbrücken)     │
│  GPIO-Pinheader für Sensoren       │
│  JST-PH Batterieanschluss (2-pol)   │
└─────────────────────────────────────┘
```

### KiCAD-Projekt

- **Aktiv:** `Sensor_Batteriebetrieb/Sensor_battery_KiCAD/Sensor_all_ESP32.kicad_pro`
- **Alt (nicht genutzt):** `_KiCAD_Backups/Sensor_all_ESP32-altenLayout/`
- **Lagen:** 2 (F.Cu + B.Cu), 1.6 mm

### Stückliste (BOM)

| Designator | Bauteil | Footprint | Wert |
|-----------|---------|-----------|------|
| U1 | ESP32-C3 SuperMini | MODULE_ESP32-C3_SUPERMINI_TH | — |
| Q1 | IRF9530 | SOT-23 | P-Kanal MOSFET (Power-Gating) |
| D1 | BZX55C3V3 | DO-35 | Zener-Diode 3.3V |
| C1 | 100µF | 0603 | Glättung |
| R1 | 10K | 0603 | Spannungsteiler |
| R2, R5 | 100K | 0603 | Spannungsteiler (Batterie-ADC) |
| R3, R4 | 4K7 | 0805 | I²C-Pullups |
| J1 | Co_x6_GND | PinHeader 1×06 | GND-Breakout |
| J2 | Co_x6_3V0 | PinHeader 1×06 | 3.3V-Breakout |
| J3 | Co_GPIOx1 | PinHeader 1×05 | GPIO-Breakout |
| J4 | Co_GPIOx2 | PinHeader 1×05 | GPIO-Breakout |
| J5 | Co_x2_battery | JST PH 2-pol | Batterieanschluss |
| J6 | Co_x1_5V0 | PinHeader 1×01 | Testpunkt |
| J7 | Co_GPIOx3 | PinHeader 1×03 | GPIO-Breakout |
| JP1, JP2 | Bridge_I2C | SolderJumper-2 | I²C-Pullup-Brücken |

### Batterie-Profile (Firmware)

| Profil | Typ | Spannung | Kapazität (typ.) |
|--------|-----|----------|-----------------|
| `BAT_PROFILE_CR2032` | CR2032 Knopfzelle | 3.0V | ~225 mAh |
| `BAT_PROFILE_2X_AA` | 2× AA Alkaline | 3.0V | ~2500 mAh |
| `BAT_PROFILE_3X_AA` | 3× AA Alkaline | 4.5V | ~2500 mAh |
| `BAT_PROFILE_2X_AAA` | 2× AAA Alkaline | 3.0V | ~1000 mAh |
| `BAT_PROFILE_3X_AAA` | 3× AAA Alkaline | 4.5V | ~1000 mAh |
| `BAT_PROFILE_LIION_1S` | Li-Ion 1S | 3.7V | ~2000 mAh |

## Pinbelegung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional (theoretisch verfügbar) |
| GPIO1 | I²C SCL | Optional (theoretisch verfügbar) |
| GPIO2 | V-Mess (ADC) | Batterie-Spannungsmessung über Spannungsteiler |
| GPIO3–10 | frei | Verfügbar über Pinheader |
| GPIO20 | RX | UART (Debug) |
| GPIO21 | TX | UART (Debug) |

## Stromspar-Design

- **Power-Gating:** P-MOSFET (IRF9530) schaltet Sensoren im Deep-Sleep komplett ab
- **Zener-Diode:** BZX55C3V3 schützt vor Überspannung (z.B. frische Batterien)
- **Spannungsteiler:** R2+R5 (100K+100K) an GPIO2 für Batterieüberwachung
- **Deep-Sleep-Strom:** Board-Gesamtstrom ~0.5–0.8 mA
- **Wake-Up:** Timer (RTC) oder externer Interrupt (z.B. Reed-Kontakt)

## Geräte dieser Klasse

| Gerät | ID | Sensoren | Batterie |
|-------|-----|----------|----------|
| [bat_sen_01](bat_sen_01_window_contact.md) | Fensterkontakt | Reed-Schalter | CR2032 |
| [bat_sen_02](bat_sen_02_rain_sensor.md) | Regensensor | ADC-Regensensor | 2× AA |
