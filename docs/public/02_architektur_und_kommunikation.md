# Architektur und Kommunikation

## Architekturgrundsatz
Das System folgt einer bewusst klaren Kommunikationsstruktur.

### Verbindliche Linie
- Geräte kommunizieren per **ESP-NOW**
- MQTT läuft nur zwischen **Master** und **Server**
- der **Master** ist die einzige Brücke
- der **Server** spricht Geräte nicht direkt an

Diese Trennung ist technisch und dokumentatorisch wichtig, weil sie die Verantwortlichkeiten sauber hält.

## Geräteschicht
Die Geräteschicht besteht aus ESP32-basierten Modulen für Sensorik und Aktorik.

Typische Rollen:
- Umweltsensorik
- Lichtsteuerung
- Rollladensteuerung
- batteriebetriebene Meldegeräte

Die Geräteschicht soll möglichst nah an Basistypen und wiederverwendbaren Fähigkeiten bleiben.

## Master
Der Master übernimmt die Brückenfunktion:
- empfängt Nachrichten aus ESP-NOW
- projiziert den Gerätestand Richtung Server
- empfängt Server-Kommandos über MQTT
- reicht relevante Bedien- oder Konfigurationssignale an Geräte weiter

Dadurch bleibt die Serverseite entkoppelt von der direkten Funkkommunikation.

## Server
Der Server verarbeitet die vom Master gelieferten Informationen.

Öffentlich sichtbare Serverlinie:
- MQTT-Ingest
- Zustandsmodell pro Gerät
- getrennte Masterdiagnose
- klare Trennung zwischen aktuellem Zustand, Logs und späteren Zeitreihen

## Fachliche Hauptwahrheit
Für die Geräteanzeige gilt:
- `state` ist die Hauptwahrheit für den sichtbaren Gerätezustand
- `event` ergänzt den Zustand
- `ack` dient der technischen Nachvollziehbarkeit

Das verhindert, dass technische Nebenpfade den eigentlichen Gerätezustand verfälschen.

## Fähigkeiten statt Einzelgerätechaos
Der Server soll Geräte nicht nur über feste Einzelnamen behandeln, sondern über technische Merkmale wie:
- `device_class`
- `caps`
- `control_mode`
- `config_profile`
- `reporting_mode`

So bleibt das Modell modular, ohne den Code unnötig zu verkomplizieren.

## Warum diese Architektur sinnvoll ist
Diese Linie ist für das Projekt passend, weil sie:
- lokale Funkkommunikation und Serverlogik sauber trennt
- den Master technisch sinnvoll positioniert
- die Serverseite neutral und erweiterbar hält
- trotzdem klein genug bleibt, um gut erklärbar zu sein

## Öffentliche Konsequenz
Das Repo soll diese Architektur nicht nur behaupten, sondern in Struktur, Dokumentation und Code sichtbar tragen.