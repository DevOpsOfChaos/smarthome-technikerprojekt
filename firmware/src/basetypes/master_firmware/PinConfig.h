/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: Master Firmware Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / ESP-NOW-MQTT-Master
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer die Master-Firmware
 Beschreibung: Legt optionale Master-Pins fuer Status-LED, Button, Relais-
 Platzhalter, I2C-Standardpins und interne Board-LED fest. Der Master arbeitet
 primaer als Funk-/MQTT-Bruecke und nutzt normalerweise keine Relais oder
 externe Sensorik. Werte von -1 deaktivieren optionale Hardware.

 Pin-Belegung:
 - Status-LED: -1, nicht bestueckt oder deaktiviert.
 - Button: -1, nicht bestueckt oder deaktiviert.
 - Relais 1/2: -1, auf dem Master nicht genutzt.
 - I2C SDA/SCL: Board-Standard aus HardwarePinStandard.h.
 - Interner NeoPixel: Board-Standard aus HardwarePinStandard.h.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Konfiguration fuer Master angelegt.
 - 2026-05-18: Dateiheader an Referenzstil angepasst.
===============================================================================
*/

#pragma once

#include "../../../include/HardwarePinStandard.h"

// Status-LED (optional). Zeigt Verbindungszustand an: AUS = Trennung, AN = verbunden.
#ifndef MASTER_PIN_STATUS_LED
#define MASTER_PIN_STATUS_LED -1
#endif

// Optionaler Reset-Taster (LOW = Werksreset-Ausloesung nach Haltezeit).
#ifndef MASTER_PIN_BUTTON_1
#define MASTER_PIN_BUTTON_1 -1
#endif

// Nicht genutzt auf dem Master (Default: -1 = deaktiviert):
#ifndef MASTER_PIN_RELAY_1
#define MASTER_PIN_RELAY_1 -1
#endif

#ifndef MASTER_PIN_RELAY_2
#define MASTER_PIN_RELAY_2 -1
#endif

#ifndef MASTER_PIN_SENSOR_SDA
#define MASTER_PIN_SENSOR_SDA SmartHome::HardwarePinStandard::PIN_I2C_SDA
#endif

#ifndef MASTER_PIN_SENSOR_SCL
#define MASTER_PIN_SENSOR_SCL SmartHome::HardwarePinStandard::PIN_I2C_SCL
#endif

#ifndef MASTER_PIN_INTERNAL_NEOPIXEL
#define MASTER_PIN_INTERNAL_NEOPIXEL SmartHome::HardwarePinStandard::GPIO_INTERNAL_NEOPIXEL
#endif

// =============================================================================
// INTERNE ALIAS-DEFINES - Mappen auf Kurznamen
// =============================================================================

constexpr int PIN_STATUS_LED = MASTER_PIN_STATUS_LED;       // Status-LED (optional)
constexpr int PIN_BUTTON_1   = MASTER_PIN_BUTTON_1;         // Reset-Taster (optional)
constexpr int PIN_RELAY_1    = MASTER_PIN_RELAY_1;          // (ungenutzt auf Master)
constexpr int PIN_RELAY_2    = MASTER_PIN_RELAY_2;          // (ungenutzt auf Master)
constexpr int PIN_SENSOR_SDA = MASTER_PIN_SENSOR_SDA;       // (ungenutzt, Standard)
constexpr int PIN_SENSOR_SCL = MASTER_PIN_SENSOR_SCL;       // (ungenutzt, Standard)
constexpr int PIN_INTERNAL_NEOPIXEL = MASTER_PIN_INTERNAL_NEOPIXEL; // Board-LED
