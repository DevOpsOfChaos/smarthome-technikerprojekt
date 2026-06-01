/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: BAT-SEN Window Contact Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer den batteriebetriebenen Fensterkontakt
 Beschreibung: Ordnet Reed-Kontakt, Batterie-ADC, Setup-Button und Setup-LED
 den konkreten ESP32-C3-Pins zu. GPIO3 ist bewusst gewaehlt, weil er Wake-faehig
 ist und nicht mit dem Boot-Button kollidiert.

 Genutzte Bibliotheken:
 - HardwarePinStandard.h: eigene Pin-Standardbibliothek fuer gemeinsam genutzte Pins.

 Pin-Belegung:
 - Fensterkontakt: GPIO3, Reed-Kontakt mit Pullup, HIGH bedeutet offen.
 - Batterie-ADC: HardwarePinStandard::PIN_BATTERY_ADC.
 - Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit.
 - Setup-LED: GPIO7, active-HIGH, 500 Millisekunden Blinkintervall.
 - Status-LED: -1, nicht bestueckt.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Mapping fuer BAT-SEN Window Contact angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

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

// Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit fuer Setup.
#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL

// Setup-Indikator-LED: GPIO7, active-HIGH, blinkt alle 500 Millisekunden.
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
