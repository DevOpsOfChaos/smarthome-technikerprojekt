# NET-ERL Kitchen

Konkreter erster Kitchen-Stand fuer den Basistyp `net_erl`.

Basis:
- Hall-Light-Ablauf mit Relais, lokaler Taste, Praesenz-Auto-On, Lux-Sperre und Auto-Off-Nachlauf
- Kitchen-Pinlinie mit LD2410C, VEML7700, BME680, ENS160, NeoPixel-Ring und Relais

Sichtbarer STATE:
- `relay_1`
- `motion`
- `lux`
- `temp_01c`
- `hum_01pct`
- `pressure_pa`
- `gas_ohm`
- `aqi`
- `tvoc_ppb`
- `eco2_ppm`
- `fault`

Hinweis:
- `gas_ohm` ist BME680-Rohgas und erst nach Warm-up belastbar.
- `aqi`, `tvoc_ppb` und `eco2_ppm` kommen aus dem ENS160.
