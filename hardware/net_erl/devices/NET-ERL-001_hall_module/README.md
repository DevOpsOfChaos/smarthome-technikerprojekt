# NET-ERL-001 — Flurmodul Basis

> 1-Relais-Knoten mit Umweltsensorik | `device_class: net_erl` | `SH_PROFILE_HALL_LIGHT`

## Übersicht

Das **NET-ERL-001** ist ein netzbetriebener 1-Relais-Knoten für die Flurbeleuchtung. Es kombiniert Lichtsteuerung mit Präsenzerkennung und Umweltsensorik.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Leistungskreis** | [leistungskreis_1relais.md](leistungskreis_1relais.md) — 1× HF46F, HLK-5M05 |
| **Steuerkreis** | [../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md) — ESP32-C3 |
| **Relais** | 1× Hongfa HF46F/5-HS1 (5V/10A) |
| **Versorgung** | 230V AC → 5V DC (HLK-5M05) |

## Sensorik

| Sensor | Typ | Schnittstelle | Adresse | Messgröße |
|--------|-----|--------------|---------|-----------|
| BME280 | Temp/Feuchte/Druck | I²C | 0x76 | Temperatur, rel. Feuchte, Luftdruck |
| VEML7700 | Luxmeter | I²C | 0x10 | Beleuchtungsstärke (0–120k Lux) |
| HC-SR501 | PIR-Bewegung | Digital (GPIO3) | — | Bewegung/Präsenz |

## GPIO-Belegung

| GPIO | Funktion | Sensor/Ziel |
|------|----------|-------------|
| GPIO0 | I²C SDA | BME280 + VEML7700 |
| GPIO1 | I²C SCL | BME280 + VEML7700 |
| GPIO3 | Digital In | HC-SR501 PIR (Bewegung) |
| GPIO10 | Trigger_PIN1 | Relais (via PC817) |
| GPIO20 | UART RX | Debug |
| GPIO21 | UART TX | Debug |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_RELAY` | 0x0001 | Relais (Licht schalten) |
| `SH_CAP_TEMP` | 0x0004 | Temperatursensor (BME280) |
| `SH_CAP_HUM` | 0x0008 | Feuchtesensor (BME280) |
| `SH_CAP_LUX` | 0x0010 | Luxsensor (VEML7700) |
| `SH_CAP_MOTION` | 0x0040 | Bewegungssensor (HC-SR501) |

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_RELAY_LIGHT` — Automatische Lichtsteuerung
- **Config Profile:** `SH_PROFILE_HALL_LIGHT` — Flurlicht-Profil
- **Reporting:** `SH_REPORTING_HYBRID` — Periodisch + Event-getrieben

## Firmware

- **Verzeichnis:** `firmware/src/devices/net_erl_hall_module/`
- **Baut mit:** `platformio run -e net_erl_hall_module`
