# Dashboard V1 Responsive Check

Datum: 2026-04-29

## Geprüfte Dateien

- `server/nodered/flows/active/60_dashboard_overview.json`
- `server/nodered/flows/active/63_dashboard_device_detail.json`
- `server/nodered/lib/dashboard_v1.js` kontrolliert, keine Logikänderung erforderlich

## Geänderte Stellen

- `60_dashboard_overview.json`: CSS im `Overview View` gehärtet.
  - `width/max-width/min-width/overflow`-Regeln für Page, Grid, Cards und Card-Inhalte ergänzt.
  - Lange Gerätenamen, Device-IDs, Highlights, Pills und Footer-Werte umbrechen jetzt mit `overflow-wrap:anywhere` bzw. `word-break`.
  - Buttons und Quick-Buttons auf touch-taugliche Mindesthöhe gebracht.
  - Mobile Breakpoints für Toolbar, Kartenkopf, Footer und Rolladen-Kompaktbedienung ergänzt.

- `63_dashboard_device_detail.json`: CSS im `Device Detail View` gehärtet.
  - `sh-relay-hero` bricht unter 720 px auf eine Spalte um.
  - `sh-cover-status-grid` nutzt auto-fit-Spalten und fällt auf Handy auf eine Spalte zurück.
  - `sh-cover-position-row` bricht auf Handy zuerst Slider plus Eingabe/Button und bei sehr kleinen Breiten vollständig einspaltig um.
  - `sh-state-row`, technische Werte, Rohdatenblöcke und Device-IDs erhalten robuste Umbruch-/Overflow-Regeln.
  - Buttons, Namenseingabe, Slider, Quick-Buttons und Lampenbutton sind auf Touch-Bedienung ausgelegt.

## Getestete Viewports

Automatisierter Chromium-Check gegen lokale Node-RED-Instanz `http://127.0.0.1:1880/dashboard/`.

Geprüfte Seiten:

- Übersicht: `/dashboard/`
- Rolladen-Detail: `/dashboard/geraet?device=NET-ZRL-001`
- Lampen-Detail: `/dashboard/geraet?device=NET-ERL-001`

Zusatz im Check: Lange Device-IDs, lange Gerätenamen, lange technische Werte und lange Rohdatenwerte wurden im Browser-DOM injiziert, um die Umbruchregeln gezielt zu belasten.

| Viewport | Ergebnis |
| --- | --- |
| 390x844 | Keine horizontale Scrollbar; Toolbar, Cards, Lampenbutton, Rolladenstatus, Positionszeile, State-Zeilen und Rohdaten umbrechen sauber. |
| 430x932 | Keine horizontale Scrollbar; Handy-Layout bleibt einspaltig und Touch-Ziele bleiben ausreichend groß. |
| 768x1024 | Keine horizontale Scrollbar; Tablet-Portrait nutzt mehrere Card-/Panel-Breiten sinnvoll, Detailinhalte bleiben lesbar. |
| 1024x768 | Keine horizontale Scrollbar; Tablet-Landscape nutzt breitere Detail- und Kartenflächen ohne gequetschte Controls. |
| 1440x900 | Keine horizontale Scrollbar; Desktop-Layout bleibt unverändert im bestehenden Look nutzbar. |

## Screenshots

Screenshots wurden lokal abgelegt unter:

- `docs/ausarbeitung/nachweise/screenshots/overview_390x844.png`
- `docs/ausarbeitung/nachweise/screenshots/overview_430x932.png`
- `docs/ausarbeitung/nachweise/screenshots/overview_768x1024.png`
- `docs/ausarbeitung/nachweise/screenshots/overview_1024x768.png`
- `docs/ausarbeitung/nachweise/screenshots/overview_desktop-1440x900.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-cover_390x844.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-cover_430x932.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-cover_768x1024.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-cover_1024x768.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-cover_desktop-1440x900.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-lamp_390x844.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-lamp_430x932.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-lamp_768x1024.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-lamp_1024x768.png`
- `docs/ausarbeitung/nachweise/screenshots/detail-lamp_desktop-1440x900.png`

## Bekannte Grenzen

- Der Check ist ein Browser-/Layoutcheck gegen die lokale Node-RED-Instanz und vorhandene SQLite-Testdaten.
- Extrem lange Werte wurden im DOM injiziert, nicht dauerhaft in der SQLite-Datenbank angelegt, damit keine Testdaten in der produktiven lokalen DB zurückbleiben.
- Es wurde kein Endgeraete-Labortest auf realen Tablets/Handys durchgeführt; der Nachweis basiert auf Chromium-Viewports.

## Bewertung Pflichtenheft

Aus Sicht dieses Checks ist die Pflichtenheft-Anforderung erfüllt: Das Dashboard V1 ist auf Desktop, Tablet und Handy ohne horizontales Scrollen nutzbar, die wichtigsten Touch-Bedienelemente sind ausreichend groß, und die kritischen Bereiche Übersicht, Detailkarten, Lampenbutton, Rolladensteuerung, technische Werte und Rohdaten sind gegen lange Inhalte und schmale Viewports gehärtet.
