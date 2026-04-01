# bat_sen

Neutraler Basistyp fuer batteriebetriebene Sensor- und Event-Nodes.

Enthalten:
- Wake/Sleep-Grundmechanik (Discovery- und RX-Fenster)
- ESP-NOW-Basisfluss (HELLO, STATE, EVENT, ACK/CFG)
- neutrale Batteriebasis (mV, Prozent, Fault)
- Device-Hooks fuer I/O, State-Kanaele, Event-Mapping und Wake-Kandidaten

Batteriebasis (V1, bewusst einfach):
- ADC-Messung ueber `PIN_BATTERY_ADC` (Standard: GPIO4 laut `HardwarePinStandard`)
- `battery_mv` aus gemittelter ADC-Messung und festem Spannungsteiler (`BATTERY_DIVIDER_NUM/DEN`)
- `battery_pct` linear zwischen `BATTERY_EMPTY_MV` und `BATTERY_FULL_MV` (Clamping auf 0..100)

Nicht enthalten:
- konkrete Fensterkontakt-Semantik
- konkrete Regensensor-Semantik
- konkrete Tasterregeln
- geraetespezifische Pin-/Default-Profile
