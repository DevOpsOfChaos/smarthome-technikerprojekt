// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-SEN Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_sen/PinConfig.h
//
// Datei-Funktion:
//   Pin-Konfiguration fuer den NET-SEN-Basistyp. Mapped Hardware-Pins
//   auf logische Funktionen (I2C, Status-LED, Setup-Button, Setup-LED).
//   Alle Werte koennen von konkreten Device-Konfigurationen
//   ueberschrieben werden (#ifndef).
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

// =============================================================================
// SENSOR-PINS – I2C-Bus fuer externe Sensoren
// =============================================================================

// I2C SDA-Pin (Default aus HardwarePinStandard.h: PIN_I2C_SDA)
#ifndef NET_SEN_PIN_SENSOR_SDA
#define NET_SEN_PIN_SENSOR_SDA SmartHome::HardwarePinStandard::PIN_I2C_SDA
#endif

// I2C SCL-Pin (Default aus HardwarePinStandard.h: PIN_I2C_SCL)
#ifndef NET_SEN_PIN_SENSOR_SCL
#define NET_SEN_PIN_SENSOR_SCL SmartHome::HardwarePinStandard::PIN_I2C_SCL
#endif

// =============================================================================
// STATUS-LED – Optional, GPIO >= 0 aktiviert sie
// =============================================================================

#ifndef NET_SEN_PIN_STATUS_LED
#define NET_SEN_PIN_STATUS_LED -1              // -1 = deaktiviert
#endif

// =============================================================================
// SETUP – Button und Status-LED fuer den Setup-/Provisioning-Modus
// =============================================================================

#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN -1                    // GPIO fuer Setup-Button (-1 = deaktiviert)
#endif

#ifndef SETUP_BUTTON_ACTIVE_LOW
#define SETUP_BUTTON_ACTIVE_LOW 1              // 1 = active-LOW (LOW = gedrueckt)
#endif

#ifndef SETUP_BUTTON_HOLD_MS
#define SETUP_BUTTON_HOLD_MS 5000UL            // Haltedauer Setup-Modus (ms)
#endif

#ifndef SETUP_INDICATOR_LED_PIN
#define SETUP_INDICATOR_LED_PIN -1             // Setup-LED GPIO (-1 = deaktiviert)
#endif

#ifndef SETUP_INDICATOR_LED_ACTIVE_HIGH
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1      // 1 = active-HIGH
#endif

#ifndef SETUP_INDICATOR_BLINK_MS
#define SETUP_INDICATOR_BLINK_MS 500UL         // Blinkintervall Setup-LED (ms)
#endif

// =============================================================================
// INTERNE ALIAS-DEFINES – Mappen auf Kurznamen
// =============================================================================

constexpr int PIN_STATUS_LED = NET_SEN_PIN_STATUS_LED;
constexpr int PIN_SENSOR_SDA = NET_SEN_PIN_SENSOR_SDA;
constexpr int PIN_SENSOR_SCL = NET_SEN_PIN_SENSOR_SCL;
