# NET-ERL-002 — KiCAD-Projekt (Geräte-Kopie)

> Kopie der Basetype-KiCAD-Dateien zur gerätespezifischen Anpassung.

## Enthaltene Platinen

| Platine | Quelle |
|---------|--------|
| [leistungskreis/](leistungskreis/) | 1-Relais-Leistungskreis (HLK-5M05, HF46F, FQP27P06) |
| [steuerkreis/](steuerkreis/) | Modularer Steuerkreis (ESP32-C3, I²C, GPIOs) |

## Anpassungen für NET-ERL-002

- [ ] I²C-Pullups (JP1/JP2) geschlossen für BME680 + VEML7700 + ENS160
- [ ] LD2410 Radar an UART (GPIO6/7)
- [ ] NeoPixel-Ring an GPIO4 (17× WS2812)
- [ ] Taster an GPIO9
