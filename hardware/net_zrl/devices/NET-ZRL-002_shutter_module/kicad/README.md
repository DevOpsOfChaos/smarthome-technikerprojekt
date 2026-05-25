# NET-ZRL-002 — KiCAD-Projekt (Geräte-Kopie)

> Kopie der Basetype-KiCAD-Dateien zur gerätespezifischen Anpassung.

## Enthaltene Platinen

| Platine | Quelle |
|---------|--------|
| [leistungskreis/](leistungskreis/) | 2-Relais-Leistungskreis (HLK-5M05, 2× HF46F, 2× IRLZ34N) |
| [steuerkreis/](steuerkreis/) | Modularer Steuerkreis (ESP32-C3, I²C, GPIOs) |

## Anpassungen für NET-ZRL-002

- [ ] 3 Taster (Hoch/Stopp/Runter) an GPIO3/4/9
- [ ] Sicherheitsschaltung: Relais-Kontakte in Serie
- [ ] Kalibrier-Fahrzeit konfigurierbar
