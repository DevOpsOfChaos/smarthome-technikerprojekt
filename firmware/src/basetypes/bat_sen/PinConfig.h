// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer BAT-SEN Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/bat_sen/PinConfig.h
//
// Datei-Funktion:
//   Pin-Konfiguration fuer den BAT-SEN-Basistyp (Batterie-Sensoren).
//   Definiert GPIOs fuer Batterie-ADC (Spannungsmessung), Wake-Eingang
//   (GPIO-Wake bei Ereignis), Status-LED und Setup-Button/LED.
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

// Status-LED (optional, -1 = deaktiviert)
#ifndef BAT_SEN_PIN_STATUS_LED
#define BAT_SEN_PIN_STATUS_LED -1
#endif

// GPIO fuer Batterie-ADC (Spannungsteiler-Eingang)
#ifndef BAT_SEN_PIN_BATTERY_ADC
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#endif

// GPIO fuer Wake-Eingang (z.B. Reed-Kontakt oder Taster) – weckt aus Deep-Sleep
#ifndef BAT_SEN_PIN_WAKE_INPUT
#define BAT_SEN_PIN_WAKE_INPUT SmartHome::HardwarePinStandard::PIN_BOOT_BUTTON
#endif

// Setup-Button (optional, -1 = deaktiviert)
#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN 2
#endif

// Button-Polaritaet: 1 = active-LOW (LOW = gedrueckt)
#ifndef SETUP_BUTTON_ACTIVE_LOW
#define SETUP_BUTTON_ACTIVE_LOW 0
#endif

// Haltedauer fuer Setup-Modus (ms)
#ifndef SETUP_BUTTON_HOLD_MS
#define SETUP_BUTTON_HOLD_MS 5000UL
#endif

// Setup-Indikator-LED
#ifndef SETUP_INDICATOR_LED_PIN
#define SETUP_INDICATOR_LED_PIN 7
#endif

// Setup-LED-Polaritaet: 1 = active-HIGH
#ifndef SETUP_INDICATOR_LED_ACTIVE_HIGH
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#endif

// Blinkintervall Setup-LED (ms)
#ifndef SETUP_INDICATOR_BLINK_MS
#define SETUP_INDICATOR_BLINK_MS 500UL
#endif

// =============================================================================
// INTERNE ALIAS-DEFINES
// =============================================================================

constexpr int PIN_STATUS_LED = BAT_SEN_PIN_STATUS_LED;
constexpr int PIN_BATTERY_ADC = BAT_SEN_PIN_BATTERY_ADC;
constexpr int PIN_WAKE_INPUT = BAT_SEN_PIN_WAKE_INPUT;
