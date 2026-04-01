# Firmware-Struktur

## Ziel

Die Firmware ist so aufgebaut, dass gemeinsame Grundlagen je Basistyp erhalten bleiben, während die eigentliche Gerätefunktion in der Geräteschicht liegt.

## Ebenen

### Gemeinsame Bibliotheken
Hier liegen nur wirklich gemeinsam genutzte technische Bausteine wie Protokoll, Kommunikationshelfer, Provisionierung, Storage und mehrfach verwendbare Sensor-Readout-Helfer.

### Basistypen
Die Basistypen enthalten nur:
- Hardware-Grundlage des Typs
- Kommunikationsgrundlage
- Grundablauf
- gemeinsame Basisfunktionen

### Geräte
Die Geräteschicht enthält:
- reale Sensoren und Aktoren
- konkrete Pinbelegung
- Default-Werte
- lokale Zustandslogik
- Sonderfunktionen des jeweiligen Geräts

## Wichtige Trennungsregel

Was nicht für jeden Vertreter eines Basistyps gilt, gehört nicht in den Basistyp.

## Wiederverwendungsregel

Gemeinsame Bibliotheken werden nur dann genutzt, wenn sie mehrfach real verwendet werden und den Code klarer machen. Gerätespezifische Logik bleibt im jeweiligen Gerätecode.
