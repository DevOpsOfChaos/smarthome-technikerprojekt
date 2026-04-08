# Cover-Command- und Positionssemantik

## Zweck

Diese Datei legt die verbindliche obere Semantik fuer Rolladen- bzw. Cover-Geraete fest.

Sie ist die normative Referenz fuer:
- Master-Projektion
- neutrale Server-Schicht
- `net_zrl`-Geraetelogik

Damit wird verhindert, dass Cover-Befehle und Positionswerte an mehreren Stellen unterschiedlich interpretiert werden.

## Geltungsbereich

Diese Semantik gilt oberhalb des Masters fuer Cover-Geraete mit `control_mode = cover`.

Interne Node-Details, Relaisbelegungen und lokale Fahrlogik bleiben davon getrennt.

## Grundregeln

- Der Master bleibt Bruecke und Projektion, keine zweite Fachlogik.
- Die neutrale Server-Schicht sendet nur kanonische Cover-Befehle.
- Der Node meldet nur echte, belastbare Zustandswerte.
- Es werden keine Platzhalter- oder Fantasiewerte fuer Positionen veroeffentlicht.

## Kanonische obere Befehle

Alle mutierenden Cover-Befehle laufen ueber `smarthome/device/<device_id>/command`.

Verpflichtende Regeln:
- mutierende Befehle enthalten immer `request_id`
- das Feld fuer die Aktion heisst immer `command`

### Oeffnen

Beispiel:
`{"command":"open","request_id":"req-123"}`

Bedeutung:
- Das Geraet soll in Richtung offen / oben fahren.
- Eine vorhandene Prozentposition ist dafuer nicht noetig.

### Schliessen

Beispiel:
`{"command":"close","request_id":"req-124"}`

Bedeutung:
- Das Geraet soll in Richtung geschlossen / unten fahren.

### Stop

Beispiel:
`{"command":"stop","request_id":"req-125"}`

Bedeutung:
- Eine laufende Fahrt soll gestoppt werden.

### Prozentanfahrt

Beispiel:
`{"command":"set_position","position":42,"request_id":"req-126"}`

Bedeutung:
- Das Geraet soll auf die Zielposition `0..100` fahren.

Verpflichtende Regeln:
- `position` ist eine Ganzzahl im Bereich `0..100`
- `set_position` ist nur erlaubt, wenn das Geraet kalibriert ist
- bei unkalibriertem Geraet wird der Befehl abgelehnt

## Obere State-Semantik

Der fachliche Zustand wird ueber `smarthome/device/<device_id>/state` veroeffentlicht.

### Pflichtfelder fuer Cover-Geraete

- `device_id`
- `cover_state`
- `cover_calibrated`
- `fault`

### Optionales Feld

- `cover_position`

`cover_position` darf nur veroeffentlicht werden, wenn die Position wirklich belastbar bekannt ist.

### Zulaessige Werte

#### `cover_state`

Zulaessige Werte sind:
- `opening`
- `closing`
- `stopped`
- `open`
- `closed`

Die genauere Ableitung bleibt beim Geraet bzw. bei der Master-Projektion des real gemeldeten Zustands.

#### `cover_position`

- Ganzzahl `0..100`
- `0` bedeutet voll geschlossen / unten
- `100` bedeutet voll offen / oben

Verboten:
- Sonderwerte ausserhalb `0..100`
- Platzhalter wie `255`
- unehrliche Nullwerte fuer unbekannte Positionen

#### `cover_calibrated`

- `true` = Prozentposition und Prozentanfahrt sind belastbar moeglich
- `false` = keine belastbare Prozentposition vorhanden

## Regel fuer unkalibrierte Geraete

Wenn ein Cover-Geraet nicht kalibriert ist:
- `open`, `close` und `stop` bleiben erlaubt
- `set_position` ist nicht erlaubt
- `cover_position` wird nicht veroeffentlicht
- `cover_calibrated` ist `false`

Damit bleibt der Zustand ehrlich, ohne zusaetzliche Sonderwerte einzufuehren.

## ACK-Semantik fuer Cover-Befehle

ACKs laufen ueber `smarthome/device/<device_id>/ack`.

Typische Statuswerte fuer Cover-Befehle sind:
- `ok`
- `busy`
- `invalid_payload`
- `unsupported`
- `not_calibrated`
- `send_failed`
- `timeout`

### Mindestregel fuer `set_position`

Wenn `set_position` an ein unkalibriertes Geraet gesendet wird, soll der Befehl nicht still ignoriert werden.

Er muss mit einem negativen ACK beantwortet werden, fachlich sinngemaess als `status = "not_calibrated"`.

## Abgrenzung der Verantwortlichkeiten

### Node

Der Node ist verantwortlich fuer:
- lokale Fahrlogik
- Kalibrierung
- echte Positionsbestimmung
- echte Zustandsmeldungen

### Master

Der Master ist verantwortlich fuer:
- Befehlsweitergabe
- Protokollprojektion
- ACK-Weitergabe
- einfache Validierung des oberen Vertrags

Der Master ist nicht verantwortlich fuer:
- Schaetzen einer unbekannten Position
- Erfinden eines Kalibrierstatus
- Komfortlogik der Rolladenfahrt

### Neutrale Server-Schicht

Die neutrale Server-Schicht ist verantwortlich fuer:
- Senden der kanonischen Cover-Befehle
- Anzeigen der publizierten Cover-Zustaende
- Respektieren von `cover_calibrated`

## Designregel

Fuer Cover gilt oberhalb des Masters immer diese Linie:
- einfache Bewegungsbefehle zuerst
- Prozentposition nur bei echter Kalibrierung
- keine Sonderwerte ausserhalb des offiziellen Positionsbereichs
- kein unehrlicher Zustand

## Kurzfassung

Verbindlich gilt:
- Cover-Befehle sind `open`, `close`, `stop`, `set_position`
- Prozentwerte sind nur `0..100`
- kein `255`
- unkalibriert bedeutet: keine `cover_position`, aber `cover_calibrated = false`
- `set_position` ist nur bei kalibriertem Geraet erlaubt
