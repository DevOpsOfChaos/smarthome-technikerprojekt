// =============================================================================
// DeviceConfig.h – Geraetekonfiguration fuer BAT-SEN Rain (Regensensor)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_rain_sensor/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       bat_sen_02
//   Name:     BAT-SEN Rain
//   Variante: bat_sen_rain_sensor
//   Caps:     BATTERY | RAIN
//
// Batterieprofil: 2x AA (2.0V-3.2V)
// Wake-Intervall: 900s (15 Minuten) – Timer-basiert, kein GPIO-Wake
// RX-Fenster:     5000ms (5s)
//
// Regen-Parameter:
//   Sample-Intervall:    200ms
//   Hysterese:          25 ADC-Stufen
//   Nass-Schwelle:      2200 (Rohwert)
//   Trocken-Schwelle:   2050 (Rohwert)
//   HIGH=wet:           ja (1)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define BAT_SEN_DEVICE_ID "bat_sen_02"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Rain"
#define BAT_SEN_FW_VARIANT "bat_sen_rain_sensor"
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_RAIN)

#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// Batterie: 2x AA (2000mV leer, 3200mV voll)
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_2X_AA
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U    // Alle 15 Min aufwachen
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U      // 5s RX-Fenster nach Wake

// GPIO-Wake: deaktiviert (V1 bleibt timer-basiert)
#define BAT_SEN_ENABLE_GPIO_WAKE 0

// Regen-Parameter
#define BAT_SEN_RAIN_SAMPLE_INTERVAL_MS 200UL   // ADC alle 200ms abfragen
#define BAT_SEN_RAIN_STATE_DELTA_RAW 25U        // Hysterese: 25 ADC-Stufen
#define BAT_SEN_RAIN_WET_THRESHOLD_RAW 2200U    // Rohwert >= 2200 = nass
#define BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW 2050U  // Rohwert <= 2050 = trocken
#define BAT_SEN_RAIN_LEVEL_HIGH_IS_WET 1        // HIGH-Pegel = nass
