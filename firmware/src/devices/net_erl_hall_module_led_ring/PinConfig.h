/*
===============================================================================
 Datei: PinConfig.h
 Code-Name: NET-ERL Hall Module LED Ring Pins
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Pin-Konfiguration / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: GPIO-Zuordnung fuer das NET-ERL Hall Module LED Ring
 Beschreibung: Ordnet I2C-Bus, Button, LD2410, NeoPixel-Ring und Relais den
 konkreten ESP32-C3-Pins zu. Der lokale Button ist gleichzeitig Setup-Button.

 Pin-Belegung:
 - I2C SDA: GPIO0 fuer BME680, VEML7700 und ENS160.
 - I2C SCL: GPIO1.
 - Button: GPIO6, active-LOW, 5000 Millisekunden Haltezeit fuer Setup.
 - LD2410 OUT: GPIO7, HIGH bedeutet Praesenz.
 - NeoPixel: GPIO8, 17 LEDs, GRB, 800 kHz.
 - Relais: GPIO10, active-HIGH.
 - LD2410 UART: GPIO20 RX, GPIO21 TX.
 - Status-/Setup-LED: -1, nicht bestueckt.

 Aenderungsverlauf:
 - 2026-05-14: Pin-Mapping fuer NET-ERL Hall Module LED Ring angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
===============================================================================
*/

#pragma once

#define PIN_SENSOR_SDA 0
#define PIN_SENSOR_SCL 1
#define PIN_BUTTON_1 6
#define PIN_LD2410_OUT 7
#define PIN_LED_RING 8
#define PIN_RELAY_1 10
#define PIN_LD2410_UART_RX 20
#define PIN_LD2410_UART_TX 21

#define LED_RING_COUNT 17
#define BUTTON_1_ACTIVE_LOW 1
#define RELAY_1_ACTIVE_HIGH 1
#define PIN_STATUS_LED -1

#define SETUP_BUTTON_PIN PIN_BUTTON_1
#define SETUP_BUTTON_ACTIVE_LOW BUTTON_1_ACTIVE_LOW
#define SETUP_BUTTON_HOLD_MS 5000UL // 5000 Millisekunden = 5 Sekunden.
#define SETUP_INDICATOR_LED_PIN -1
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL // 500 Millisekunden, nur relevant wenn LED bestueckt ist.
