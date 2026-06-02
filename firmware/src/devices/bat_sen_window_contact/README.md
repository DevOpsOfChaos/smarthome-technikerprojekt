# bat_sen_window_contact

Erstes konkretes Device auf dem neutralen `bat_sen`-Basistyp.

## Enthaelt

- feste Device-Kennung und Device-Defaults
- festen Kontakt-Pin (`GPIO3`) inkl. Wake-Pin-Zuordnung
- einfache Kontaktsemantik: `open` oder `closed`
- Event-Mapping auf `SH_EVENT_WINDOW_OPENED` und `SH_EVENT_WINDOW_CLOSED`
- C3-GPIO-Deep-Sleep-Wakeup ueber den Device-Pin
- RTC-Statusspeicher fuer Fenster-Events nach Deep-Sleep-Wakeup
- GPIO8-Board-LED/WS2812 bleibt ungenutzt und wird vom BAT-SEN-Basistyp ausgeschaltet
- Batterieprofil: 2x AAA in Reihe (`BAT_PROFILE_2X_AAA`, 2000 bis 3200 mV)

## Enthaelt bewusst nicht

- Regenlogik
- Profil-Multiplex
- geraetespezifische Sonderpfade ausser Fensterkontakt

## Wake-Strategie (C3)

- GPIO-Wakeup ist auf RTC-faehige C3-Pins begrenzt (GPIO0..GPIO5).
- Das Device nutzt deshalb bewusst `GPIO3` statt Boot-Button-Pin `GPIO9`.
- Wake ist level-basiert, die Firmware setzt den Zielpegel aber vor jedem
  Deep-Sleep passend zum aktuellen Kontaktzustand.
- Geschlossen -> offen und offen -> geschlossen wecken dadurch jeweils sofort.
- Das periodische Timer-Wake-Intervall liegt bei 12 Stunden. Es dient nur
  Batterie-/Alive-Meldungen, nicht der Kontakterkennung.
- Der letzte Kontaktzustand bleibt im RTC-Speicher, damit der Wake auch als
  Fenster-Event gemeldet werden kann.

## Setup-Taster

- Kurzer Druck toggelt Stay-awake im Normalbetrieb: aktiv bedeutet, das Geraet
  geht nicht in Deep-Sleep.
- Erneuter kurzer Druck erlaubt Deep-Sleep wieder; nach Ablauf des RX-Fensters
  darf das Geraet dann wieder schlafen.
- 5 Sekunden halten startet den Setup-Modus.
