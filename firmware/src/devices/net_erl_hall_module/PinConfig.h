/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: NET-ERL Hall Module Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer das NET-ERL Hall Module
 Beschreibung: Ordnet Relais, PIR-Sensor, I2C-Bus, Setup-Button und Setup-LED
 den konkreten ESP32-C3-Pins zu. Status-LED ist bei dieser Variante nicht
 bestueckt.

 Pin-Belegung:
 - Relais: GPIO10, active-HIGH.
 - PIR-Sensor: GPIO7, HIGH bedeutet Bewegung.
 - I2C SDA: GPIO0 fuer BME280 und VEML7700.
 - I2C SCL: GPIO1.
 - Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit.
 - Setup-LED: GPIO6, active-HIGH, 500 Millisekunden Blinkintervall.
 - Status-LED: -1, nicht bestueckt.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Mapping fuer NET-ERL Hall Module angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
===============================================================================
*/

#pragma once

// Relais- und Sensorpins sind direkte GPIO-Nummern des ESP32-C3. Aenderungen
// muessen zur realen Verdrahtung passen; der Compiler kann hier keine Hardware
// pruefen.
#define PIN_RELAY_1 10
#define PIN_SENSOR_SDA 0
#define PIN_SENSOR_SCL 1
#define PIN_PIR 7
// -1 bedeutet "nicht bestueckt"; Runtime-Code ueberspringt pinMode/digitalWrite.
#define PIN_STATUS_LED -1
// active-HIGH: HIGH zieht das Relais an. Bei active-LOW-Relais muss dieser Wert 0 sein.
#define RELAY_1_ACTIVE_HIGH 1

// Setup-Button und Setup-LED werden vom NET-ERL-Basistyp gelesen. Der Button ist
// active-LOW, daher normalerweise mit Pullup betrieben.
#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL // 5000 Millisekunden = 5 Sekunden.
#define SETUP_INDICATOR_LED_PIN 6
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL // 500 Millisekunden Blinkintervall.
