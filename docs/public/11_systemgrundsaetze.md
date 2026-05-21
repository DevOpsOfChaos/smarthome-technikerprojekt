# Systemgrundsätze

Dieses Repository bildet die offizielle technische Projektlinie des Smart-Home-Technikerprojekts ab.

## Grundausrichtung

Das System wird lokal betrieben und ist modular aufgebaut. Es soll nachvollziehbar, testbar und für die Technikerarbeit sauber dokumentierbar sein.

## Technische Leitlinien der Hauptlinie

- Dezentrale Geräte kommunizieren per ESP-NOW mit dem Master.
- MQTT wird nur zwischen Master und Server verwendet.
- Der Master ist die einzige Brücke zwischen Geräteebene und Server-Ebene.
- Der Server greift nicht direkt auf die Geräte zu.
- Die Gerätefunktion liegt primär in der jeweiligen Firmware des Geräts.
- Relevante Laufzeitwerte werden lokal über Provisionierung verwaltet.
- Alles andere bleibt fest im Code und wird bei Bedarf neu geflasht.

## ESPHome-Alternative

Neben der Hauptlinie gibt es eine direkte MQTT-Alternative unter `esphome/`.

Für diese Linie gilt:
- ESPHome baut und flasht die Geräte.
- Die Geräte sprechen direkt MQTT.
- ESP-NOW und Master werden nicht genutzt.
- Der gemeinsame Serververtrag bleibt die fachliche Klammer.

Diese Alternative ist bewusst getrennt dokumentiert, damit die Hauptarchitektur nicht verwässert wird.

## Projektfokus

Diese Projektlinie verfolgt bewusst eine schlanke und erklärbare Architektur. Ziel ist kein universelles Produktframework, sondern eine belastbare prototypische Umsetzung der Kernfunktionen.

## Öffentliche Repository-Regel

In diesem Repository werden keine sensiblen Daten und keine internen Arbeitsmittel dokumentiert. Öffentliche Dokumentation und Code bleiben technisch und projektbezogen.
