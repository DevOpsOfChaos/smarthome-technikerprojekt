// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer BAT-SEN Rain (Regensensor)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_rain/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   Regensensor (ADC):  GPIO3  – Analoger Regensensor-Signalpin (0-4095)
//   Batterie-ADC:       GPIO?  – definiert in HardwarePinStandard
//   Setup-Button:       GPIO2  – active-LOW (LOW = gedrueckt)
//   Setup-LED:          GPIO7  – active-HIGH, blinkt im Setup-Modus (500ms)
//   Status-LED:         -1     – nicht bestueckt
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

// Regensensor-Signalpin (ADC-Eingang, 0-4095, 12-Bit)
#define BAT_SEN_RAIN_SIGNAL_PIN 3

// Status-LED nicht bestueckt
#define BAT_SEN_PIN_STATUS_LED -1

// Batterie-ADC (Spannungsteiler-Eingang)
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC

// Wake-Eingang = Regensensor-Pin (Timer-Wake, nicht GPIO-Wake)
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_RAIN_SIGNAL_PIN

// Setup-Button (GPIO2, active-LOW, 5s Hold fuer Setup-Modus)
#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 0
#define SETUP_BUTTON_HOLD_MS 5000UL

// Setup-Indikator-LED (GPIO7, active-HIGH, blinkt 500ms)
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
