# Node-RED Flows

## Ziel

Die Flows werden so getrennt, dass klar erkennbar ist, was zur aktiven Basislinie gehört und was vorläufig pausiert ist.

## Ordner

- `active/` enthält Flows, die Teil des normalen Basisbetriebs sind.
- `paused/` enthält Flows, die bewusst nicht aktiv genutzt werden.

## Regel

Pausierte Flows dürfen den aktiven Pflichtpfad nicht beeinflussen.

## Basislinie

Aktiv sind nur die Flows, die für Datenempfang, Visualisierung, einfache Bedienung und stabile Grundfunktion des Systems benötigt werden.
