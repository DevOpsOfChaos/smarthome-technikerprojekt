/*
===============================================================================
 Datei: DebugConfig.h
 Code-Name: DebugConfig
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: Zentrale Debug-Schalter
 Beschreibung: Legt fest, welche Debug-Ausgaben beim Entwickeln aktiv sind.

 Genutzte Bibliotheken:
 - Keine importierte Bibliothek. Diese Datei stellt nur Konstanten bereit.

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/
#pragma once

// Master-Schalter: true = Debug-Ausgaben aktiv
constexpr bool DEBUG_AKTIV           = true;

// Subsystem-Debug-Schalter (derzeit alle ungenutzt).
// Nur wirksam wenn DEBUG_AKTIV == true.
// Bei Bedarf pro Subsystem aktivieren.

// Reserviert, derzeit ungenutzt. Nur wirksam wenn DEBUG_AKTIV == true.
constexpr bool DEBUG_SENSORIK        = true;   // Sensormesswerte und I2C-Diagnose
// Reserviert, derzeit ungenutzt. Nur wirksam wenn DEBUG_AKTIV == true.
constexpr bool DEBUG_KOMMUNIKATION   = true;   // ESP-NOW- und MQTT-Pakete
// Reserviert, derzeit ungenutzt. Nur wirksam wenn DEBUG_AKTIV == true.
constexpr bool DEBUG_AKTOREN         = true;   // Relais-Schaltvorgaenge
