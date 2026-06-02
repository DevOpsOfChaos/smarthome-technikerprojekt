// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer BAT-SEN Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/bat_sen/PinConfig.h
//
// Datei-Funktion:
//   Pin-Konfiguration fuer den BAT-SEN-Basistyp (Batterie-Sensoren).
//   Definiert GPIOs fuer Batterie-ADC (Spannungsmessung), Wake-Eingang
//   (GPIO-Wake bei Ereignis), Status-LED, Board-LED/NeoPixel-Pin und
//   Setup-Button/LED.
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

// Board-LED/WS2812-Datenpin auf ESP32-C3-Boards.
// BAT-SEN nutzt diese LED nicht; der Pin wird aktiv AUS gesetzt, damit keine
// blaue Board-LED dauerhaft Batterie verbraucht.
#ifndef BAT_SEN_PIN_BOARD_LED_NEOPIXEL
#define BAT_SEN_PIN_BOARD_LED_NEOPIXEL SmartHome::HardwarePinStandard::GPIO_INTERNAL_NEOPIXEL
#endif

// 1 = LED aus bei LOW. Falls ein konkretes Board active-LOW verdrahtet ist,
// muss das Device diesen Wert auf 0 ueberschreiben.
#ifndef BAT_SEN_BOARD_LED_ACTIVE_HIGH
#define BAT_SEN_BOARD_LED_ACTIVE_HIGH 1
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
#define SETUP_BUTTON_ACTIVE_LOW 1
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
constexpr int PIN_BOARD_LED_NEOPIXEL = BAT_SEN_PIN_BOARD_LED_NEOPIXEL;
constexpr int PIN_BATTERY_ADC = BAT_SEN_PIN_BATTERY_ADC;
constexpr int PIN_WAKE_INPUT = BAT_SEN_PIN_WAKE_INPUT;
constexpr bool BOARD_LED_AKTIV_HIGH = BAT_SEN_BOARD_LED_ACTIVE_HIGH != 0;
