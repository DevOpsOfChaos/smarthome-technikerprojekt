/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: BAT-SEN Rain Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer den batteriebetriebenen Regensensor
 Beschreibung: Definiert Identitaet, Faehigkeiten, Batterieprofil, Timer-Wake
 und ADC-Schwellwerte fuer bat_sen_rain_sensor. Das Geraet bleibt bewusst
 timer-basiert und nutzt keinen GPIO-Wake.

 Genutzte Bibliotheken:
 - DeviceTypes.h: eigene Protokollbibliothek mit Capability-Flags und Batterieprofilen.

 Wichtige Werte:
 - 900 Sekunden Wake-Intervall entsprechen 15 Minuten.
 - 5000 Millisekunden RX-Fenster entsprechen 5 Sekunden Empfangszeit nach Wake.
 - 200 Millisekunden Sample-Intervall begrenzen die ADC-Abfrage.
 - 25 ADC-Stufen Hysterese reduzieren Flattern am Grenzwert.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer BAT-SEN Rain angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define BAT_SEN_DEVICE_ID "bat_sen_02"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Rain"
#define BAT_SEN_FW_VARIANT "bat_sen_rain_sensor"
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_RAIN)

// BAT_SEN-Devices verwenden BAT_SEN_*-Praefix ohne _DEVICE_-Infix
// (anders als NET_ERL_DEVICE_* und NET_SEN_DEVICE_*).
#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// Batterie: 2x AA (2000mV leer, 3200mV voll)
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_2X_AA
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U    // 900 Sekunden = 15 Minuten.
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U      // 5000 Millisekunden = 5 Sekunden.

// GPIO-Wake: deaktiviert (V1 bleibt timer-basiert)
#define BAT_SEN_ENABLE_GPIO_WAKE 0

// Regen-Parameter
#define BAT_SEN_RAIN_SAMPLE_INTERVAL_MS 200UL   // ADC alle 200 Millisekunden abfragen.
#define BAT_SEN_RAIN_STATE_DELTA_RAW 25U        // Hysterese: 25 ADC-Stufen
#define BAT_SEN_RAIN_WET_THRESHOLD_RAW 2200U    // Rohwert >= 2200 = nass
#define BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW 2050U  // Rohwert <= 2050 = trocken
#define BAT_SEN_RAIN_LEVEL_HIGH_IS_WET 1        // HIGH-Pegel = nass
