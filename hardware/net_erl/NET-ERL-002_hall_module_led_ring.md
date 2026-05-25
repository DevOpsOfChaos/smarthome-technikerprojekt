# NET-ERL-002 — Flurmodul LED-Ring

> 1-Relais-Knoten mit erweiterter Sensorik + LED-Ring | `SH_PROFILE_HALL_MODULE_LED_RING`

## Übersicht

Das **NET-ERL-002** ist die erweiterte Variante des Flurmoduls. Zusätzlich zum Basis-Modul verfügt es über Gassensorik (BME680, ENS160), Radar-Präsenzerkennung (LD2410) und einen NeoPixel-LED-Ring für visuelles Feedback.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Leistungskreis** | [leistungskreis_1relais.md](leistungskreis_1relais.md) — 1× HF46F, HLK-5M05 |
| **Steuerkreis** | [../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md) — ESP32-C3 |
| **Relais** | 1× Hongfa HF46F/5-HS1 |
| **LED-Ring** | 12× WS2812 NeoPixel (GPIO8) |
| **Versorgung** | 230V AC → 5V DC (HLK-5M05) |

## Sensorik

| Sensor | Typ | Schnittstelle | Adresse | Messgröße |
|--------|-----|--------------|---------|-----------|
| BME680 | Temp/Feuchte/Druck/Gas | I²C | 0x76/0x77 | T, rH, p, VOC-Gaswiderstand |
| VEML7700 | Luxmeter | I²C | 0x10 | Beleuchtungsstärke |
| ENS160 | MOX-Gassensor | I²C | 0x52/0x53 | eCO₂, TVOC, AQI-Index |
| LD2410 | mmWave-Radar | UART | GPIO6(RX)/GPIO7(TX) | Präsenz, stationär/bewegt |

## GPIO-Belegung

| GPIO | Funktion | Sensor/Ziel |
|------|----------|-------------|
| GPIO0 | I²C SDA | BME680 + VEML7700 + ENS160 |
| GPIO1 | I²C SCL | BME680 + VEML7700 + ENS160 |
| GPIO6 | UART RX | LD2410 Radar |
| GPIO7 | UART TX | LD2410 Radar |
| GPIO8 | NeoPixel Data | WS2812 LED-Ring (12 LEDs) |
| GPIO9 | Taster | Bedientaster (mit Pullup) |
| GPIO10 | Trigger_PIN1 | Relais (via PC817) |
| GPIO20 | UART RX | Debug/Programmierung |
| GPIO21 | UART TX | Debug/Programmierung |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_RELAY` | 0x0001 | Relais |
| `SH_CAP_TEMP` | 0x0004 | Temperatur (BME680) |
| `SH_CAP_HUM` | 0x0008 | Feuchte (BME680) |
| `SH_CAP_LUX` | 0x0010 | Lux (VEML7700) |
| `SH_CAP_MOTION` | 0x0040 | Bewegung (LD2410 Radar) |
| `SH_CAP_AQI` | 0x0020 | Luftqualität (ENS160) |
| `SH_CAP_GAS` | 0x4000 | VOC-Gas (BME680) |
| `SH_CAP_PRESSURE` | 0x8000 | Luftdruck (BME680) |
| `SH_CAP_BUTTON` | 0x0400 | Taster (GPIO9) |
| `SH_CAP_LED_RING` | 0x1000 | NeoPixel-Ring (GPIO8) |

## LED-Ring

- 12× WS2812B NeoPixel, angesteuert über GPIO8
- **Achtung:** GPIO8 wird auch von der onboard RGB-LED genutzt. Im Gerät wird der LED-Ring priorisiert.
- Funktionen: Statusanzeige, Helligkeits-Feedback, Farbcodierung für AQI

## LD2410 Radar

- 24GHz mmWave-Sensor für Präsenz- und Bewegungserkennung
- UART-Schnittstelle (GPIO6 RX, GPIO7 TX)
- Konfigurierbare Empfindlichkeit und Reichweite (0.75–6m)
- Erkennt stationäre und bewegte Personen

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_RELAY_LIGHT`
- **Config Profile:** `SH_PROFILE_HALL_MODULE_LED_RING`
- **Reporting:** `SH_REPORTING_HYBRID`

## Firmware

- **Verzeichnis:** `firmware/src/devices/net_erl_hall_module_led_ring/`
- **Baut mit:** `platformio run -e net_erl_hall_module_led_ring`
