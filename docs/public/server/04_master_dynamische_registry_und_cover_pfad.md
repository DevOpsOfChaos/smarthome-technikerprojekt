# Master-Ziellinie: dynamische Registry und Cover-Pfad

## Zweck

Diese Datei beschreibt eine Ziel- und Folgestufe.
Sie ist nicht die Hauptbeschreibung des aktuellen kleinen Server-V1-Kerns.

Der aktuelle oeffentliche V1-Stand liegt bei:
- Compose-Inline-Start ueber `server/docker-compose.yml`
- engem Ingest-/Store-/Persistenzkern
- Dashboard V1 mit Uebersicht und versteckter Detailseite pro Geraet

Diese Datei beschreibt die verbindliche Ziellinie fuer den Master-Umbau.

Sie soll verhindern, dass der Master weiter als statische Pilot-Bridge fuer einzelne bekannte Geraete fortgefuehrt wird.

Der Master soll auf einen Stand gebracht werden, der:
- neue reale Nodes dynamisch aufnehmen kann
- Cover-Geraete sauber tragen kann
- fuer die neutrale Server-Schicht eine saubere obere Projektion liefert

## Was am aktuellen Master falsch ist

Der bisherige Master-Stand ist fuer den weiteren Projektpfad zu eng:
- statische Geraeteliste statt dynamischer Registry
- keine saubere obere Cover-Befehlsebene
- zu stark auf Pilotgeraete zugeschnitten
- Pending-/ACK-Modell zu klein fuer echten Mehrgeraetebetrieb

Dieser Zustand wird nicht weitergefuehrt.

## Zielbild des Masters

Der Master bleibt:
- Bridge zwischen ESP-NOW und MQTT
- Projektion des oberen Geraetezustands
- Weiterleiter von Commands und ACKs

Der Master wird nicht:
- zweite Logikplattform
- Komfortinstanz fuer Geraeteverhalten
- schaetzende oder erfindende Fachlogik

## Dynamische Registry

Der Master fuehrt eine dynamische Laufzeit-Registry.

### Grundregel

Nodes werden nicht mehr ueber eine feste `NODE_DEFINITIONS`-Liste zugelassen.

Stattdessen gilt:
- ein gueltiges `HELLO` mit gueltiger `device_id`
- bekannte `device_class`
- gueltiger `power_type`
- gueltige Absender-MAC

ist die Grundlage fuer die Aufnahme in die Registry.

### Pro Registry-Eintrag

Der Master haelt mindestens:
- `device_id`
- `device_name`
- `device_class`
- `power_type`
- `mac_address`
- `fw_version`
- `caps`
- `meta_schema_version`
- `control_mode`
- `config_profile`
- `reporting_mode`
- `sensor_mask`
- `input_mask`
- `last_seen`
- `online` / `availability`
- letzten bekannten State

### Ableitungsregel

Offline-/Availability-Logik wird aus dem echten Registry-Eintrag abgeleitet, nicht aus harter Geraeteliste.

## MQTT-Oberpfad

Der Master publiziert fuer jedes dynamisch bekannte Geraet weiterhin:
- `meta`
- `availability`
- `state`
- `event`
- `ack`

Der Master konsumiert weiterhin:
- `smarthome/device/<device_id>/command`

## Cover-Pfad

Fuer Geraete mit `control_mode = cover` gilt die in
`03_cover_command_und_positionssemantik.md`
beschriebene Semantik.

Der Master muss dafuer oben verstehen:
- `open`
- `close`
- `stop`
- `set_position`

### Validierungsregeln

- `open`, `close` und `stop` sind auch ohne Prozentposition erlaubt
- `set_position` ist nur bei kalibriertem Geraet erlaubt
- der Master erfindet keine Position und keinen Kalibrierstatus
- unkalibrierte Geraete werden fuer `set_position` negativ bestaetigt

## Pending- und ACK-Modell

Der Master verwendet kein einziges globales Pending-Slot-Modell mehr.

Stattdessen gilt:
- Pending-Zustand pro Geraet
- ACK-Zuordnung pro Geraet und Sequenz
- Timeout und Retry pro Geraet
- ein laufender Command auf Geraet A blockiert nicht Geraet B

## Regeln fuer State-Projektion

### Allgemein

Der Master publiziert nur Werte, die er wirklich vom Node weiss.

Er publiziert nicht:
- erfundene Negativwerte
- Fantasiepositionen
- angeblich bestaetigte Konfiguration ohne echte Grundlage

### Fuer Cover

Oben relevant sind mindestens:
- `cover_state`
- `cover_calibrated`
- optional `cover_position`
- `fault`

`cover_position` wird nur publiziert, wenn sie belastbar bekannt ist.

## Reihenfolge fuer die Umsetzung

### Schritt 1
Statische Geraeteliste aus dem Master entfernen.

### Schritt 2
Dynamische Registry fuer `HELLO`-basierte Node-Aufnahme einbauen.

### Schritt 3
MQTT-Command-Parsing auf dynamische Registry statt feste Geraeteliste umstellen.

### Schritt 4
Cover-Commands `open` / `close` / `stop` / `set_position` in den Master aufnehmen.

### Schritt 5
Pending-/ACK-Modell auf pro-Geraet-Basis umstellen.

### Schritt 6
Danach erst `net_zrl` auf diesen Vertrag fertigziehen.

## Abgrenzung

Diese Datei ist die Ziellinie fuer den Master.

Sie ist keine Freigabe dafuer,
- wieder alte Altprojektlogik einzubauen
- den Master mit Komfort- oder Serverlogik aufzublasen
- unsaubere Mischpfade fuer Pilotgeraete zu behalten

## Kurzfassung

Verbindlich gilt:
- keine feste Geraeteliste mehr
- dynamische Master-Registry
- Cover-Befehle direkt im Master-Vertrag
- Pending-/ACK-Modell pro Geraet
- der Master bleibt Bridge, nicht Fachlogik-Instanz
