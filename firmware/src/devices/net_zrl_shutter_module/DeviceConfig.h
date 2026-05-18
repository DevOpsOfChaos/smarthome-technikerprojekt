/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: NET-ZRL Shutter Module Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Netzbetriebener Rollo-Aktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer das NET-ZRL Shutter Module
 Beschreibung: Ueberschreibt die Defaults aus dem net_zrl-Basistyp fuer den
 konkreten Rollo-Aktor. Alle nicht gesetzten Werte werden weiterhin aus dem
 Basistyp geerbt.

 Hardware:
 - ESP32-C3
 - Zwei Relais fuer Auf und Ab
 - Drei lokale Taster fuer Auf, Ab und Stop
 - Zwei Status-LEDs

 Genutzte Bibliotheken:
 - Keine zusaetzlichen Header in dieser Datei; der Basistyp wertet die Defines aus.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer NET-ZRL Shutter Module angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
===============================================================================
*/

#pragma once

// Geraete-Identifikation.
#define NET_ZRL_DEVICE_ID           "NET-ZRL-002"
#define NET_ZRL_DEVICE_NAME         "NET-ZRL Shutter"
#define NET_ZRL_FW_VARIANT          "net_zrl_shutter_module"

// Debug-Ausgaben fuer Entwicklung und Inbetriebnahme.
#define NET_ZRL_DEBUG_ENABLED       1

// Relais-Pin-Mapping.
#define NET_ZRL_RELAY_UP_PIN        10
#define NET_ZRL_RELAY_DOWN_PIN      5
#define NET_ZRL_RELAY_UP_ACTIVE_HIGH    1
#define NET_ZRL_RELAY_DOWN_ACTIVE_HIGH  1

// Taster-Pins.
#define NET_ZRL_BUTTON_UP_PIN       2
#define NET_ZRL_BUTTON_DOWN_PIN     4
#define NET_ZRL_BUTTON_STOP_PIN     3

// LED-Pins.
#define NET_ZRL_LED_UP_PIN          6
#define NET_ZRL_LED_DOWN_PIN        7
#define NET_ZRL_LED_ACTIVE_HIGH     1

// Pegel-Logik.
#define NET_ZRL_BUTTON_ACTIVE_LOW   0
