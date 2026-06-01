# bat_sen_window_contact

Erstes konkretes Device auf dem neutralen `bat_sen`-Basistyp.

## Enthaelt

- feste Device-Kennung und Device-Defaults
- festen Kontakt-Pin (`GPIO3`) inkl. Wake-Pin-Zuordnung
- einfache Kontaktsemantik: `open` oder `closed`
- Event-Mapping auf `SH_EVENT_WINDOW_OPENED` und `SH_EVENT_WINDOW_CLOSED`
- C3-GPIO-Deep-Sleep-Wakeup ueber den Device-Pin

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

## Setup-Taster

- Kurzer Druck toggelt Stay-awake: aktiv bedeutet, das Geraet geht nicht in
  Deep-Sleep.
- Erneuter kurzer Druck erlaubt Deep-Sleep wieder.
- 5 Sekunden halten startet den Setup-Modus.
