// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-ERL Hall Module
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_hall_module/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// Pin-Mapping fuer Hall-Modul mit Bewegungsmelder.
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   Relais:         GPIO10  – active-HIGH
//   PIR-Sensor:     GPIO6   – Eingang (HIGH = Bewegung)
//   I2C SDA:        GPIO0   – BME280 (0x76) + VEML7700 (0x10)
//   I2C SCL:        GPIO1
//   Status-LED:     -1      – nicht bestueckt
//   Setup-Button:   GPIO2   – active-LOW
//   Setup-LED:      GPIO7   – active-HIGH
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#define PIN_RELAY_1 10
#define PIN_SENSOR_SDA 0
#define PIN_SENSOR_SCL 1
#define PIN_PIR 6
#define PIN_STATUS_LED -1
#define RELAY_1_ACTIVE_HIGH 1

#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
