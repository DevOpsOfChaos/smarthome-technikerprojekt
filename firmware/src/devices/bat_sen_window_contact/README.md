# bat_sen_window_contact

Erstes konkretes Device auf dem neutralen `bat_sen`-Basistyp.

Enthaelt:
- feste Device-Kennung und Device-Defaults
- festen Kontakt-Pin inkl. Wake-Pin-Zuordnung
- einfache Kontaktsemantik: `open` oder `closed`
- Event-Mapping auf `SH_EVENT_WINDOW_OPENED` und `SH_EVENT_WINDOW_CLOSED`

Enthaelt bewusst nicht:
- Regenlogik
- Profil-Multiplex
- geraetespezifische Sonderpfade ausser Fensterkontakt
