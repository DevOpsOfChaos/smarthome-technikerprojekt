/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: BAT-SEN Rain Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer den batteriebetriebenen Regensensor
 Beschreibung: Ordnet ADC-Regensensor, Batterie-ADC, Setup-Button und Setup-LED
 den konkreten ESP32-C3-Pins zu. Der Regensensor-Pin wird auch als Wake-Input
 eingetragen, GPIO-Wake ist fuer diese Variante aber in DeviceConfig.h deaktiviert.

 Genutzte Bibliotheken:
 - HardwarePinStandard.h: eigene Pin-Standardbibliothek fuer gemeinsam genutzte Pins.

 Pin-Belegung:
 - Regensensor: GPIO3, ADC-Rohwert 0 bis 4095.
 - Batterie-ADC: HardwarePinStandard::PIN_BATTERY_ADC.
 - Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit.
 - Setup-LED: GPIO7, active-HIGH, 500 Millisekunden Blinkintervall.
 - Status-LED: -1, nicht bestueckt.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Mapping fuer BAT-SEN Rain angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

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

// Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit fuer Setup.
#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 0
#define SETUP_BUTTON_HOLD_MS 5000UL

// Setup-Indikator-LED: GPIO7, active-HIGH, blinkt alle 500 Millisekunden.
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
