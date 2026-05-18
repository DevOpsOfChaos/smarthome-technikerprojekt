// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-ERL Hall Module LED Ring
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_hall_module_led_ring/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// Pin-Mapping fuer Hall-Modul mit Radar-Praesenz, Luftqualitaet und LED-Ring.
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA:      GPIO0  – BME680 (0x76), VEML7700 (0x10), ENS160 (0x52)
//   I2C SCL:      GPIO1
//   Button:       GPIO6  – active-LOW, 40ms Debounce, 5s Hold = Setup
//   LD2410 OUT:   GPIO7  – HIGH = Praesenz erkannt
//   NeoPixel:     GPIO8  – 17 LEDs, GRB, 800kHz
//   Relais:       GPIO10 – active-HIGH
//   LD2410 UART:  GPIO20 (RX), GPIO21 (TX)
//   Status-LED:   -1    – nicht bestueckt
//
// Setup-Button = gleicher Pin wie Button (GPIO6, nur langes Halten)
// Setup-LED = -1 (keine separate LED fuer Setup)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

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
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN -1
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
