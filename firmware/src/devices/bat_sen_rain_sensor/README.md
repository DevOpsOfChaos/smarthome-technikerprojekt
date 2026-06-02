# BAT-SEN Regensensor

Konkretes BAT-SEN-Geraet mit einfachem ADC-Regenpfad.

## V1-Semantik

- `channel_bool_1`: `1 = wet`, `0 = dry`
- `channel_u16_1`: ADC-Rohwert (`rain_raw`)
- Event bei Zustandswechsel:
  - `event_type = SH_EVENT_RAIN_DETECTED`
  - `param1 = 1` bei Wechsel nach `wet`
  - `param1 = 0` bei Wechsel nach `dry`
  - `param2 = aktueller ADC-Rohwert`

## Hinweise

- Das Protokoll hat aktuell nur `SH_EVENT_RAIN_DETECTED`.
- Deshalb signalisiert `param1` den Zielzustand des Wechsels.
- Schwelle ist als vorlaeufiger V1-Default gesetzt und muss mit echter Hardware validiert werden.
- V1 bleibt timer-basiert (kein GPIO-Wake).
- GPIO8-Board-LED/WS2812 bleibt ungenutzt und wird vom BAT-SEN-Basistyp ausgeschaltet.
