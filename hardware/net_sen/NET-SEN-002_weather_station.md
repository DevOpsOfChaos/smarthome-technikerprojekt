# NET-SEN-002 — Wetterstation

> Netzbetriebene Außen-Wetterstation | `device_class: net_sen`

## Übersicht

Die **NET-SEN-002** ist eine netzbetriebene Wetterstation für den Außenbereich. Sie misst Temperatur, Luftfeuchte, Luftdruck, Helligkeit und Regen.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Platine** | [README.md](README.md) — Sensor_Netzbetrieb Standard Version |
| **Netzteil** | HLK-PM03 (230V AC → 3.3V DC) |
| **Versorgung** | 230V AC, dauerhaft |

## Sensorik

| Sensor | Typ | Schnittstelle | Adresse | Messgröße |
|--------|-----|--------------|---------|-----------|
| BME280 | Temp/Feuchte/Druck | I²C | 0x76 | Temperatur, rel. Feuchte, Luftdruck |
| VEML7700 | Luxmeter | I²C | 0x10 | Beleuchtungsstärke |
| Digitaler Regen | Regen | Digital (GPIO) | — | Regen ja/nein |

## GPIO-Belegung

| GPIO | Funktion | Sensor/Ziel |
|------|----------|-------------|
| GPIO0 | I²C SDA | BME280 + VEML7700 |
| GPIO1 | I²C SCL | BME280 + VEML7700 |
| GPIO5 | Digital In | Regensensor |
| GPIO20 | UART RX | Debug |
| GPIO21 | UART TX | Debug |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_TEMP` | 0x0004 | Temperatur (BME280) |
| `SH_CAP_HUM` | 0x0008 | Feuchte (BME280) |
| `SH_CAP_LUX` | 0x0010 | Lux (VEML7700) |
| `SH_CAP_PRESSURE` | 0x8000 | Luftdruck (BME280) |
| `SH_CAP_RAIN` | 0x0100 | Regensensor |

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_NONE`
- **Config Profile:** `SH_PROFILE_NONE`
- **Reporting:** `SH_REPORTING_HYBRID`

## Firmware

- **Verzeichnis:** `firmware/src/devices/net_sen_weather_station/`
- **Baut mit:** `platformio run -e net_sen_weather_station`
