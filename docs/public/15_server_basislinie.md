# Server-Basislinie

## Aufgabe des Servers

Die Server-Ebene verarbeitet und visualisiert die Daten des Systems und ermöglicht einfache Bedienfunktionen.

## Aktive Bestandteile der Basislinie

- MQTT-Broker
- Node-RED
- Empfang und Verarbeitung der Gerätedaten
- Live-Dashboard
- Anzeige aktueller Sensorwerte
- Online-/Offline-Anzeige
- einfache Aktorsteuerung
- einfache Rolladenbedienung

## Optional aktiv, wenn stabil

- Datenspeicherung von Messwerten
- Wetterintegration

## Vorläufig pausiert

- serverseitige Konfigurationssteuerung
- Automatisierungsregeln als aktiver Pflichtpfad
- allgemeine Regel-Engine
- zeitbasierte Szenen
- serverseitige Soll-/Ist-Konfigurationslogik

## Grundsatz

Der Server unterstützt die Geräteebene, übernimmt aber nicht die primäre Geräteintelligenz. Das konkrete Verhalten der Geräte liegt in der jeweiligen Gerätefirmware.
