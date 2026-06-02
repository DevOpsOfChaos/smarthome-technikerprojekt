# bat_sen_01 — Fensterkontakt

> Batteriebetriebener Fenster-/Türkontakt | `device_class: bat_sen`

## Übersicht

Der **bat_sen_01** ist ein batteriebetriebener Fenster- und Türkontakt. Er verwendet einen Reed-Schalter (Magnetschalter) und sendet nur bei Zustandsänderung.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Platine** | [README.md](README.md) — Sensor_Batteriebetrieb |
| **Sensor** | Reed-Schalter (magnetischer Näherungsschalter) |
| **Batterie** | 2x AAA in Serie (~3.0V nominal) |
| **Betrieb** | Deep-Sleep mit Wake-Up bei Reed-Änderung |

## GPIO-Belegung

| GPIO | Funktion |
|------|----------|
| GPIO2 | Setup-Button |
| GPIO3 | Reed-Schalter (Digital In, Wake-Up fähig) |
| GPIO4 | V-Mess (Batterie-ADC) |
| GPIO20/21 | UART (nur Debug) |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_BATTERY` | 0x0200 | Batteriebetrieb |
| `SH_CAP_WINDOW` | 0x0080 | Fensterkontakt |

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_NONE`
- **Battery Profile:** `BAT_PROFILE_2X_AAA`
- **Reporting:** `SH_REPORTING_SLEEP_EVENT` — Sendet nur bei Zustandsänderung

## Wake-Up-Verhalten

1. **Normalzustand:** ESP32 in Deep-Sleep, Reed-Schalter überwacht
2. **Fenster geöffnet/geschlossen:** Reed-Schalter ändert Zustand → GPIO3 Interrupt → ESP wacht auf
3. **Senden:** Batteriestatus + Fensterzustand per ESP-NOW an Master
4. **Rückkehr:** ESP geht zurück in Deep-Sleep

## Firmware

- **Verzeichnis:** `firmware/src/devices/bat_sen_window_contact/`
- **Baut mit:** `platformio run -e bat_sen_window_contact`
