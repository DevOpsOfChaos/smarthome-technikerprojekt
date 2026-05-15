// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-ERL Kitchen Light
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_kitchen_light/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   Setup-Button:    GPIO2 – active-LOW (LOW = gedrueckt)
//   Setup-LED:       GPIO3 – active-HIGH, blinkt 500ms im Setup-Modus
//
// Status: PLATZHALTER – Pins sind vorbereitet, DeviceConfig.h ist leer.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN 3
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
