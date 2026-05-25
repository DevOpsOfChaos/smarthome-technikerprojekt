# Architektur und Kommunikation

## Architekturgrundsatz
Das System folgt einer bewusst klaren Kommunikationsstruktur.

### Hauptlinie: eigene Firmware
- Geräte kommunizieren per **ESP-NOW**
- MQTT läuft nur zwischen **Master** und **Server**
- der **Master** ist die einzige Brücke
- der **Server** spricht Geräte nicht direkt an

Diese Trennung ist technisch und dokumentatorisch wichtig, weil sie die Verantwortlichkeiten sauber hält.

### Alternative Linie: ESPHome
Für Nutzer, die ESPHome bevorzugen, gibt es zusätzlich eine direkte MQTT-Linie:
- Geräte werden mit ESPHome gebaut und geflasht
- Geräte verbinden sich per WLAN direkt mit dem MQTT-Broker
- der Master und ESP-NOW entfallen
- Topic-Pfade und Payload-Felder bleiben auf den Serververtrag ausgerichtet

Diese Alternative ist bewusst kein Mischbetrieb im selben Kommunikationspfad. Sie ist ein zweiter Gerätepfad mit demselben Serververtrag.

## Geräteschicht
Die Geräteschicht besteht aus ESP32-basierten Modulen für Sensorik und Aktorik.

Typische Rollen:
- Umweltsensorik
- Lichtsteuerung
- Rollladensteuerung
- batteriebetriebene Meldegeräte

Die Geräteschicht soll möglichst nah an Basistypen und wiederverwendbaren Fähigkeiten bleiben.

## Master
In der eigenen Firmware-Linie übernimmt der Master die Brückenfunktion:
- empfängt Nachrichten aus ESP-NOW
- projiziert den Gerätestand Richtung Server
- empfängt Server-Kommandos über MQTT
- reicht relevante Bedien- oder Konfigurationssignale an Geräte weiter

Der Master erkennt Gerätetypen nicht über feste Gerätenamen, ID-Präfixe oder Payload-Längen. `HELLO` ist die verbindliche Quelle für Klasse, Fähigkeiten und Profile. Wenn ein Gerät zuerst nur `HEARTBEAT` oder `STATE` sendet, fordert der Master per `HELLO_REQUEST` die Metadaten an und wartet mit dem Parsen, bis diese vorliegen.

Dadurch bleibt die Serverseite entkoppelt von der direkten Funkkommunikation.

## Server
Der Server verarbeitet die per MQTT gelieferten Informationen.

Öffentlich sichtbare Serverlinie:
- MQTT-Ingest
- Zustandsmodell pro Gerät
- getrennte Masterdiagnose
- klare Trennung zwischen aktuellem Zustand, Logs und späteren Zeitreihen

In der Hauptlinie kommen die MQTT-Nachrichten vom Master. In der ESPHome-Alternative kommen sie direkt vom jeweiligen Gerät. Der Server soll fachlich denselben Vertrag sehen.

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

Die ESPHome-Alternative ist sinnvoll, weil sie:
- den Einstieg für ESPHome-Nutzer erleichtert
- vorhandene Home-Assistant-/ESPHome-Werkzeuge nutzt
- Geräte schneller über YAML anpassbar macht
- trotzdem den gemeinsamen MQTT-Vertrag des Projekts respektiert

Die Entscheidung ist deshalb nicht "richtig oder falsch", sondern eine Frage des Ziels: eigene Firmware für maximale Kontrolle und saubere Projektarchitektur, ESPHome für schnellen praktischen Einstieg mit vorhandenen ESPHome-Werkzeugen.

## Öffentliche Konsequenz
Das Repo soll diese Architektur nicht nur behaupten, sondern in Struktur, Dokumentation und Code sichtbar tragen.
