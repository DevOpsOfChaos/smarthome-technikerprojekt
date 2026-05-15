// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer Master
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/master_firmware/PinConfig.h
//
// Datei-Funktion:
//   Pin-Konfiguration fuer den ESP-NOW-Master. Der Master nutzt keine
//   Relais und aktuell keine externe Sensorik. Feste Board-Standards
//   bleiben trotzdem zentral hinterlegt. Optionale Status-LED zeigt
//   Verbindungszustand an (AN=verbunden, AUS=Trennung).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

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
// INTERNE ALIAS-DEFINES – Mappen auf Kurznamen
// =============================================================================

constexpr int PIN_STATUS_LED = MASTER_PIN_STATUS_LED;       // Status-LED (optional)
constexpr int PIN_BUTTON_1   = MASTER_PIN_BUTTON_1;         // Reset-Taster (optional)
constexpr int PIN_RELAY_1    = MASTER_PIN_RELAY_1;          // (ungenutzt auf Master)
constexpr int PIN_RELAY_2    = MASTER_PIN_RELAY_2;          // (ungenutzt auf Master)
constexpr int PIN_SENSOR_SDA = MASTER_PIN_SENSOR_SDA;       // (ungenutzt, Standard)
constexpr int PIN_SENSOR_SCL = MASTER_PIN_SENSOR_SCL;       // (ungenutzt, Standard)
constexpr int PIN_INTERNAL_NEOPIXEL = MASTER_PIN_INTERNAL_NEOPIXEL; // Board-LED
