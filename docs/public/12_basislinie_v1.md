# Basislinie V1

## Zweck

Die Basislinie V1 definiert den Stand, der für die offizielle Projektlinie zuerst belastbar aufgebaut, getestet und dokumentiert werden soll.

## Enthaltene Systemteile

### Basistypen
- `master`
- `net_zrl`
- `net_sen`
- `net_erl`
- `bat_sen`

### Geplante konkrete Geräte
- Master als ESP-NOW- zu MQTT-Brücke
- `net_zrl_shutter` als Rolladensteuerung
- `net_sen_env_bme280_veml_rain` als netzbetriebenes Umwelt- und Regenmodul
- `net_erl_hall_light` als Flurlicht
- `net_erl_kitchen` als Küchenmodul
- `bat_sen_window_contact` als Fensterkontakt
- `bat_sen_rain` als batteriebetriebener Regensensor

## Ziele der Basislinie

- klare und schlanke Firmware-Struktur
- verständliche Trennung zwischen Basistyp und Geräteschicht
- saubere Provisionierung relevanter Laufzeitwerte
- Dashboard mit Anzeige aktueller Zustände und einfacher Bedienung
- nachvollziehbare Test- und Nachweisführung

## Nicht Teil der aktiven Basislinie

- serverseitige Konfigurationssteuerung
- allgemeine Automatisierungsregeln als Pflichtpfad
- übergreifende Komfort- und Sonderlogik
- unnötig breite Abstraktionsschichten

## Begründung

Die Basislinie V1 priorisiert eine belastbare, dokumentierbare und erklärbare Kernarchitektur. Weitergehende Funktionen können später darauf aufbauen, ohne den Kern unnötig zu verkomplizieren.
