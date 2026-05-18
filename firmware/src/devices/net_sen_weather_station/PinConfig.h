/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: NET-SEN Weather Station Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / Netzbetriebener Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer die netzbetriebene Wetterstation
 Beschreibung: Ordnet I2C-Bus, digitalen Regensensor, Setup-Button und Setup-LED
 den konkreten ESP32-C3-Pins zu. Status-LED ist bei dieser Variante nicht bestueckt.

 Pin-Belegung:
 - I2C SDA: GPIO0 fuer BME280 und VEML7700.
 - I2C SCL: GPIO1.
 - Regen-Digital: GPIO3, active-LOW mit Pullup.
 - Setup-Button: GPIO2, active-LOW, 5000 Millisekunden Haltezeit.
 - Setup-LED: GPIO7, active-HIGH, 500 Millisekunden Blinkintervall.
 - Status-LED: -1, nicht bestueckt.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Mapping fuer NET-SEN Weather Station angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
===============================================================================
*/

#pragma once

#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1

// Digitaler Regen-Signalpin (active-LOW, Pullup aktiv)
#define NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN 3
#define NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW 1
#define NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP 1

#define NET_SEN_PIN_STATUS_LED -1

#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL // 5000 Millisekunden = 5 Sekunden.
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL // 500 Millisekunden Blinkintervall.
