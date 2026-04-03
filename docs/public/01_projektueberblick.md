# Projektüberblick

## Ziel des Projekts
Dieses Projekt ist ein lokales, modulares Smart-Home-System im Rahmen eines Technikerprojekts.

Der Fokus liegt auf:
- klarer technischer Architektur
- nachvollziehbarer Trennung der Kommunikationswege
- wiederverwendbaren Basistypen
- sauber dokumentierter Entwicklung
- einem System, das technisch erklärbar und praktisch testbar bleibt

## Systemidee in Kurzform
Das Gesamtsystem besteht aus drei Hauptschichten:

1. **Dezentrale Geräte**
   - Sensor- und Aktormodule auf ESP32-Basis
   - Kommunikation mit dem Master ausschließlich per ESP-NOW

2. **Master**
   - zentrale Brücke zwischen Geräteschicht und Server
   - einzige Stelle, die ESP-NOW und MQTT verbindet

3. **Server**
   - MQTT-Ingest
   - Node-RED-basierte Verarbeitung und Visualisierung
   - Datenhaltung für Zustände, Historien und später Zeitreihen

## Grundprinzipien
- Nodes sprechen nicht direkt mit dem Server.
- MQTT ist kein zweiter Gerätebus, sondern die Verbindung zwischen Master und Server.
- Der Master bleibt die einzige Brücke.
- Geräte sollen über Basistypen und Fähigkeiten sauber beschreibbar bleiben.
- Der Code soll modular sein, aber nicht unnötig abstrakt.

## Basistypen
Aktuell folgt das Projekt dieser Basistyp-Linie:
- `master`
- `net_erl`
- `net_zrl`
- `net_sen`
- `bat_sen`

Konkrete Geräte werden auf dieser Grundlage beschrieben, statt für jedes Sondergerät eine völlig neue Serverlogik einzuziehen.

## Öffentliche Zielrichtung
Das öffentliche Repo zeigt die offizielle technische Projektlinie.
Es dient als sauberer, nachvollziehbarer und präsentierbarer Projektstand.

## Was dieses Repo bewusst nicht sein soll
- Sammelbecken für interne Hilfsdateien
- Experimentablage ohne klare Linie
- Ort für sensible Daten
- künstlich aufgeblähte Architektur-Spielwiese

## Nächster sinnvoller Einstieg
Für den aktuellen Serverstand anschließend lesen:
- `02_architektur_und_kommunikation.md`
- `03_aktueller_status_und_naechste_schritte.md`
- `04_server_schnellstart_phase1.md`