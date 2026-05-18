/*
===============================================================================
 Datei: main.cpp
 Code-Name: NET-ZRL Shutter Module
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Netzbetriebener Rollo-Aktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-15
 Letzte Bearbeitung: 2026-05-18

 Zweck: Device-Adapter fuer die Rollo-Steuerung
 Beschreibung: Dieser Thin-Wrapper bindet nur die konkrete Device-Konfiguration
 ein und uebergibt danach an den net_zrl-Basistyp. Die eigentliche Logik fuer
 Relaisverriegelung, Taster, Fahrzeiten, Positionsmodell, ESP-NOW und MQTT liegt
 in firmware/src/basetypes/net_zrl/main.cpp.

 Hardware:
 - ESP32-C3
 - Zwei Relais fuer Auf und Ab
 - Drei Taster fuer lokale Bedienung
 - Zwei Status-LEDs

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-/ESP32-Grundlagen, die vom eingebundenen Basistyp erwartet werden.
 - DeviceConfig.h: eigene Device-Konfiguration fuer Pins, Zeiten und Rollo-Verhalten.
 - net_zrl/main.cpp: eigener Basistyp; liefert setup(), loop() und die komplette
   Rollo-Laufzeitlogik.

 Aenderungsverlauf:
 - 2026-05-15: Thin-Wrapper fuer NET-ZRL Shutter Module angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/

#include <Arduino.h>

#include "DeviceConfig.h"

// Basistyp einbinden: liefert setup(), loop() und die gesamte Rollo-Steuerung.
#include "../../basetypes/net_zrl/main.cpp"
