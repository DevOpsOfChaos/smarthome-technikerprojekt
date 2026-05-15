// =============================================================================
// DeviceConfig.h – Geraetekonfiguration fuer BAT-SEN Window Contact
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_window_contact/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       bat_sen_01
//   Name:     BAT-SEN Window
//   Variante: bat_sen_window_contact
//   Caps:     BATTERY | WINDOW
//
// Batterieprofil: CR2032 (2.2V-3.0V)
// Wake-Intervall: 900s (15 Min) – Timer + GPIO-Wake bei Kontaktwechsel
// RX-Fenster:     5000ms (5s)
//
// Kontakt-Parameter:
//   Entprellzeit:             35ms
//   Input-Pullup:             aktiv
//   Offen-Pegel:              HIGH
//   GPIO-Wake:                aktiv (HIGH = Wake)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define BAT_SEN_DEVICE_ID "bat_sen_01"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Window"
#define BAT_SEN_FW_VARIANT "bat_sen_window_contact"
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_WINDOW)

#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// Batterie: CR2032 (2200mV leer, 3000mV voll)
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_CR2032
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U

// GPIO-Wake aktiv: wird bei Pegelwechsel am Kontakt-Pin aufgeweckt
#define BAT_SEN_ENABLE_GPIO_WAKE 1
// Wake-Bedingung: offen = HIGH (= gleicher Pegel wie Kontakt offen)
#define BAT_SEN_GPIO_WAKE_LEVEL_HIGH BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH

// Kontakt-Parameter
#define BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS 35UL      // Entprellzeit (ms)
#define BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP 1    // Pullup aktiv
#define BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH 1     // offen = HIGH
