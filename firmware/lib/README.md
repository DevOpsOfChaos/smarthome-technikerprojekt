# Gemeinsame Bibliotheken

In diesem Ordner liegen nur wirklich gemeinsam genutzte technische Bausteine.

## Typische Inhalte

- Protokollstrukturen
- ESP-NOW-Helfer
- MQTT-Helfer
- Provisionierung
- Storage
- kleine Utilities
- Sensor-Readout-Helfer bei mehrfach gleicher Nutzung

## Nicht vorgesehen

- gerätespezifische Sonderlogik
- Komfortlogik einzelner Geräte
- allgemeine Profil- oder Regel-Engines
- lokale Zustandsmaschinen konkreter Geräte

## Grundsatz

Wiederverwendung ist nur sinnvoll, wenn sie den Code tatsächlich klarer und mehrfach nutzbar macht. Gerätespezifische Logik bleibt in der jeweiligen Geräteschicht.
