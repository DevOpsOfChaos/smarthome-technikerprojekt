# bat_sen

Neutraler Basistyp fuer batteriebetriebene Sensor- und Event-Nodes.

Enthalten:
- Wake/Sleep-Grundmechanik (Discovery- und RX-Fenster)
- ESP-NOW-Basisfluss (HELLO, STATE, EVENT, ACK/CFG)
- neutrale Batteriebasis (mV, Prozent, Fault)
- Device-Hooks fuer I/O, State-Kanaele, Event-Mapping und Wake-Kandidaten
- GPIO8-Board-LED/WS2812-Pin wird aktiv ausgeschaltet, weil BAT-SEN ihn nicht nutzt

Batteriebasis (V1, bewusst einfach):
- ADC-Messung ueber `PIN_BATTERY_ADC` (Standard: GPIO4 laut `HardwarePinStandard`)
- `battery_mv` aus gemittelter ADC-Messung und festem Spannungsteiler (`BATTERY_DIVIDER_NUM/DEN`)
- Default-Profil: 2x AA, linear 2000mV bis 3200mV
- Messketten-Kalibrierung: `BAT_SEN_BATTERY_CALIBRATION_NUM/DEN`
- `battery_pct` linear zwischen `BATTERY_EMPTY_MV` und `BATTERY_FULL_MV` (Clamping auf 0..100)
- Default-Node-ID: `BAT-SEN-001` (protokollgueltig: Grossbuchstaben/Ziffern/Bindestrich)

Board-LED/WS2812:
- Viele ESP32-C3-Boards fuehren eine blaue Board-LED oder einen WS2812-Pixel auf
  `GPIO8`.
- BAT-SEN behandelt `GPIO8` nicht als Statusanzeige. Der Pin wird beim Start und
  direkt vor Deep-Sleep auf AUS gesetzt, damit kein Dauerstrom ueber die LED
  fliesst.
- Wenn ein konkretes Board die LED active-LOW verdrahtet, muss
  `BAT_SEN_BOARD_LED_ACTIVE_HIGH` im Device-Pinmapping auf `0` gesetzt werden.

Nicht enthalten:
- konkrete Fensterkontakt-Semantik
- konkrete Regensensor-Semantik
- konkrete Tasterregeln
- geraetespezifische Pin-/Default-Profile
