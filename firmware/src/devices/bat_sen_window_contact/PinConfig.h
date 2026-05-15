// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer BAT-SEN Window Contact
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_window_contact/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   Fensterkontakt:      GPIO3 – Reed-Kontakt, INPUT_PULLUP
//                              GPIO3 ist C3-Wake-faehig (GPIO0-5),
//                              bewusst NICHT GPIO9 (Boot-Button)
//   Batterie-ADC:        GPIO? – HardwarePinStandard::PIN_BATTERY_ADC
//   Setup-Button:        GPIO2 – active-LOW
//   Setup-LED:           GPIO7 – active-HIGH
//   Status-LED:          -1    – nicht bestueckt
//
// Wake-Konfiguration:
//   GPIO-Wake:           aktiv (BAT_SEN_ENABLE_GPIO_WAKE=1)
//   Wake-Pegel:          HIGH (BAT_SEN_GPIO_WAKE_LEVEL_HIGH=1)
//   Fenster offen =      HIGH (BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH=1)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../include/HardwarePinStandard.h"

// Fensterkontakt-Pin (GPIO3, C3-Wake-faehig, Pullup aktiv)
// GPIO3 ist wake-faehig (C3: GPIO0..GPIO5) und trennt den
// Fensterkontakt sauber vom Boot-Button-Standardpin GPIO9.
#define BAT_SEN_WINDOW_CONTACT_PIN 3

// Status-LED nicht bestueckt
#define BAT_SEN_PIN_STATUS_LED -1

// Batterie-ADC
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC

// Wake-Input = Fensterkontakt-Pin (GPIO-Wake bei Pegelwechsel)
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_WINDOW_CONTACT_PIN

// Setup-Button (GPIO2, active-LOW)
#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 0
#define SETUP_BUTTON_HOLD_MS 5000UL

// Setup-Indikator-LED (GPIO7, active-HIGH, blinkt 500ms)
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
