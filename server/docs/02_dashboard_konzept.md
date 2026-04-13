# Dashboard-Konzept

## Seiten

- ├£bersicht
- Ger├ñtedetail als kontextuelle Unterseite

## Ziel

- neue Ger├ñte sofort sichtbar
- responsive Grundstruktur f├╝r Handy und Desktop
- Seitenwechsel prim├ñr ├╝ber das FlowFuse-Seitenmen├╝
- Ger├ñtedetail nur als kontextuelle Unterseite, nicht als Hauptnavigation
- echte Ger├ñte- und Statusdaten statt Deko
- `sim_*`-Serverdaten separat sichtbar machen, ohne reale Pilot-IDs in dieselbe Sicht zu ziehen

## Bedienregeln

- ├£bersichtskarten lesen Verf├╝gbarkeit kanonisch aus `availability`
- Ger├ñtedetail liest dieselbe kanonische Availability und zeigt `state`, `meta`, `event`, `ack`, `config` und `diagnostics` roh
- `net_zrl` bekommt auf der Detailseite enge Zusatzlogik f├╝r Rollladenstatus, aber keinen separaten Command-Pfad
- Ger├ñteklassifikation bleibt klein und leitet sich prim├ñr aus `device_class`, `base_type` und wenigen Metafeldern ab
- Bedienung wird nicht vorget├ñuscht: ohne aktiven Command-Pfad bleibt die Oberfl├ñche lesend

## Bewusst offen

- Wetter
- Diagramme
- Logs
- Automationen
- Schreibpfade fuer Commands oder Dashboard-Konfiguration
