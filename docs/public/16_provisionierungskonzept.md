# Provisionierungskonzept

## Grundidee

Provisionierung dient der lokalen Einrichtung und Anpassung weniger sinnvoller Laufzeitwerte. Sie ersetzt nicht den Gerätecode.

## Geeignete Werte

Typische Provisionierungswerte sind:
- Gerätename
- Master-Bindung
- Kalibrierwerte
- Schwellwerte
- Nachlaufzeiten
- Mess- oder Sleep-Intervalle

## Nicht vorgesehen

Nicht über Provisionierung geändert werden:
- Sensorauswahl
- Pinbelegung
- Gerätegrundfunktion
- Basistyp
- komplette Gerätearchitektur

## Rolle im Gesamtsystem

Die Provisionierung verwaltet die Werte, die im laufenden Betrieb plausibel geändert werden können. Alles andere bleibt fest im Code und wird bei Bedarf neu geflasht.

## Setup

Die lokale Einrichtung erfolgt gerätenah. Dadurch bleiben Geräteverhalten und Zuständigkeiten klar nachvollziehbar.
