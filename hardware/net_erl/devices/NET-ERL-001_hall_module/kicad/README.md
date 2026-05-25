# NET-ERL-001 — KiCAD-Projekt (Geräte-Kopie)

> Kopie der Basetype-KiCAD-Dateien zur gerätespezifischen Anpassung.

## Enthaltene Platinen

| Platine | Quelle |
|---------|--------|
| [leistungskreis/](leistungskreis/) | 1-Relais-Leistungskreis (HLK-5M05, HF46F, FQP27P06) |
| [steuerkreis/](steuerkreis/) | Modularer Steuerkreis (ESP32-C3, I²C, GPIOs) |

## Anpassungen für NET-ERL-001

- [ ] I²C-Pullups (JP1/JP2) geschlossen für BME280 + VEML7700
- [ ] GPIO3 als PIR-Eingang beschalten
- [ ] Kein LD2410, kein NeoPixel, kein ENS160
