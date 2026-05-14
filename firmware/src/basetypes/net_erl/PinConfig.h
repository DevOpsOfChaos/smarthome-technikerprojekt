// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-ERL Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_erl/PinConfig.h
//
// Datei-Funktion:
//   Pin-Konfiguration fuer den NET-ERL-Basistyp. Mapped Hardware-Pins
//   auf logische Funktionen (Relais, Status-LED, Setup-Button).
//   Alle Werte koennen von konkreten Device-Konfigurationen
//   ueberschrieben werden (#ifndef).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Änderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include "../../../include/HardwarePinStandard.h"

// =============================================================================
// RELAIS – GPIO fuer Relais 1 und Polaritaet
// =============================================================================

// GPIO-Pin fuer Relais 1 (Default aus HardwarePinStandard.h)
#ifndef NET_ERL_PIN_RELAY_1
#define NET_ERL_PIN_RELAY_1 SmartHome::HardwarePinStandard::PIN_RELAY_1
#endif

// Polaritaet des Relais: 1 = active-HIGH (HIGH = EIN), 0 = active-LOW (LOW = EIN)
#ifndef NET_ERL_RELAY_1_ACTIVE_HIGH
#define NET_ERL_RELAY_1_ACTIVE_HIGH 1
#endif

// =============================================================================
// STATUS-LED – Optional, GPIO >= 0 aktiviert sie
// =============================================================================

// GPIO fuer optionale Status-LED (leuchtet synchron mit Relais)
// -1 = deaktiviert (keine Status-LED)
#ifndef NET_ERL_PIN_STATUS_LED
#define NET_ERL_PIN_STATUS_LED -1
#endif

// =============================================================================
// SETUP – Button und Status-LED fuer den Setup-/Provisioning-Modus
// =============================================================================

// GPIO fuer Setup-Button (Gedrueckt-Halten startet Setup-Modus)
// -1 = deaktiviert (kein Hardware-Setup-Button)
#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN -1
#endif

// Button-Polaritaet: 1 = active-LOW (LOW = gedrueckt), 0 = active-HIGH
#ifndef SETUP_BUTTON_ACTIVE_LOW
#define SETUP_BUTTON_ACTIVE_LOW 1
#endif

// Haltedauer in ms bis Setup-Modus erkannt wird (Default: 5s)
#ifndef SETUP_BUTTON_HOLD_MS
#define SETUP_BUTTON_HOLD_MS 5000UL
#endif

// GPIO fuer Setup-Indikator-LED (blinkt im Setup-Modus)
// -1 = deaktiviert
#ifndef SETUP_INDICATOR_LED_PIN
#define SETUP_INDICATOR_LED_PIN -1
#endif

// Setup-LED-Polaritaet: 1 = active-HIGH, 0 = active-LOW
#ifndef SETUP_INDICATOR_LED_ACTIVE_HIGH
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#endif

// Blinkintervall der Setup-Indikator-LED (ms)
#ifndef SETUP_INDICATOR_BLINK_MS
#define SETUP_INDICATOR_BLINK_MS 500UL
#endif

// =============================================================================
// INTERNE ALIAS-DEFINES – Mappen NET_ERL_*-Praeﬁxe auf Kurznamen
// =============================================================================

#define PIN_RELAY_1 NET_ERL_PIN_RELAY_1
#define PIN_STATUS_LED NET_ERL_PIN_STATUS_LED
#define RELAY_1_ACTIVE_HIGH NET_ERL_RELAY_1_ACTIVE_HIGH
