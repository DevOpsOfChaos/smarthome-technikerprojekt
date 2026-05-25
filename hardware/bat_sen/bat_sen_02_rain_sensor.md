# bat_sen_02 — Regensensor

> Batteriebetriebener Regensensor | `device_class: bat_sen`

## Übersicht

Der **bat_sen_02** ist ein batteriebetriebener Regensensor. Er verwendet einen analogen Regensensor (Widerstandsänderung bei Nässe) mit Hysterese und sendet periodisch oder bei Regenbeginn.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Platine** | [README.md](README.md) — Sensor_Batteriebetrieb |
| **Sensor** | Analoger Regensensor (Widerstandsplatte) |
| **Batterie** | 2× AA Alkaline (2500 mAh, ~3.0V) |
| **Betrieb** | Deep-Sleep mit Timer-Wake-Up |

## GPIO-Belegung

| GPIO | Funktion |
|------|----------|
| GPIO2 | V-Mess (Batterie-ADC) |
| GPIO4 | Regensensor (Analog In, ADC) |
| GPIO20/21 | UART (nur Debug) |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_BATTERY` | 0x0200 | Batteriebetrieb |
| `SH_CAP_RAIN` | 0x0100 | Regensensor |

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_NONE`
- **Battery Profile:** `BAT_PROFILE_2X_AA`
- **Reporting:** `SH_REPORTING_SLEEP_EVENT` — Sendet bei Regenänderung + periodisch

## Messprinzip

- **Regensensor:** Analoge Widerstandsplatte — je nasser, desto niedriger der Widerstand
- **Hysterese:** Schwellwerte für "trocken → Regen" und "Regen → trocken" verhindern Flattern
- **ADC-Messung:** GPIO4 liest Spannung am Spannungsteiler (Sensor + Pullup)

## Firmware

- **Verzeichnis:** `firmware/src/devices/bat_sen_rain_sensor/`
- **Baut mit:** `platformio run -e bat_sen_rain_sensor`
