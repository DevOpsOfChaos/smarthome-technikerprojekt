/*
===============================================================================
 Datei: HardwarePinStandard.h
 Code-Name: HardwarePinStandard
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: Verbindlicher Pinstandard
 Beschreibung: Sammelt die gemeinsam festgelegten ESP32-C3-Pins fuer Sensoren, Relais und Onboard-Funktionen.

 Genutzte Bibliotheken:
 - Keine importierte Bibliothek. Diese Datei stellt nur Pin-Konstanten bereit.

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/
#pragma once

namespace SmartHome {
namespace HardwarePinStandard {

// --- I2C-Bus (wird von allen net_sen- und net_erl-Devices genutzt) ---
constexpr int PIN_I2C_SDA           = 0;   // I2C Data (SDA)
constexpr int PIN_I2C_SCL           = 1;   // I2C Clock (SCL)

// --- Onboard-Peripherie ---
constexpr int GPIO_INTERNAL_NEOPIXEL = 8;  // Interne NeoPixel/WS2812 LED (optional)
constexpr int PIN_BOOT_BUTTON        = 9;  // Boot-Button (active-LOW, Strapping-Pin!)

// --- Relais-Ausgaenge ---
constexpr int PIN_RELAY_1           = 10;  // Relais 1 (Standard-Aktor-Pin)
constexpr int PIN_RELAY_2           = 5;   // Relais 2 (z.B. Rollo-Auf/Ab)

// --- Analog-Eingaenge ---
constexpr int PIN_BATTERY_ADC       = 4;   // Batterie-Spannungsmessung (ADC, bat_sen-Devices)

}  // namespace HardwarePinStandard
}  // namespace SmartHome
