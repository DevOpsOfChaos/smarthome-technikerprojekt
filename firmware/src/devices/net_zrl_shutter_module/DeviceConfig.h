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
 Beschreibung: Enthalten sind nur Werte fuer dieses konkrete Device. Das Modul
 bindet keinen Basistyp ein; alle hier gesetzten Defines werden direkt von
 main.cpp ausgewertet.

 Hardware:
 - ESP32-C3
 - Zwei Relais fuer Auf und Ab
 - Drei lokale Taster fuer Auf, Ab und Stop
 - Zwei Status-LEDs

 Genutzte Bibliotheken:
 - Keine zusaetzlichen Header in dieser Datei.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer NET-ZRL Shutter Module angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
 - 2026-05-19: Konfiguration fuer eigenstaendige Device-Firmware bereinigt.
===============================================================================
*/

#pragma once

// DeviceTypes.h liefert Capability-, Profil- und Control-Mode-Konstanten fuer
// den HELLO-Payload des eigenstaendigen Shutter-Moduls.
#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Geraete-Identifikation. NET_ZRL_DEVICE_ID ist die stabile Registry-/MQTT-ID;
// nicht kosmetisch aendern, sonst entsteht fuer den Master ein neues Geraet.
#define NET_ZRL_DEVICE_ID           "NET-ZRL-002"
#define NET_ZRL_DEVICE_NAME         "NET-ZRL Shutter"
#define NET_ZRL_FW_VARIANT          "net_zrl_shutter_module"

// Geraete-Faehigkeiten, Steuermodus, Profil und Meldeverhalten.
// RELAY/RELAY2 sind intern vorhanden, werden masterseitig aber ueber COVER-
// Kommandos gesteuert. SH_CAP_COVER ist deshalb die fachlich wichtigste Cap.
#define NET_ZRL_DEVICE_CAPS             (SH_CAP_RELAY | SH_CAP_RELAY2 | SH_CAP_COVER | SH_CAP_MULTIBUTTON)
#define NET_ZRL_DEVICE_CONTROL_MODE     SH_CONTROL_MODE_COVER
#define NET_ZRL_DEVICE_CONFIG_PROFILE   SH_PROFILE_COVER_BASIC
#define NET_ZRL_DEVICE_REPORTING_MODE   SH_REPORTING_HYBRID

// Debug-Ausgaben fuer Entwicklung und Inbetriebnahme.
#define NET_ZRL_DEBUG_ENABLED       1 // Compile-Time-Schalter fuer serielle Debugausgaben.

// Relais-Pin-Mapping. Up/Down duerfen niemals gleichzeitig aktiv sein; main.cpp
// erzwingt Dead-Time und gegenseitiges Abschalten.
#define NET_ZRL_RELAY_UP_PIN        10
#define NET_ZRL_RELAY_DOWN_PIN      5
// active-HIGH: HIGH am GPIO zieht das jeweilige Relais an.
#define NET_ZRL_RELAY_UP_ACTIVE_HIGH    1
#define NET_ZRL_RELAY_DOWN_ACTIVE_HIGH  1

// Taster-Pins fuer lokale Bedienung. active-LOW ist unten separat gesetzt.
#define NET_ZRL_BUTTON_UP_PIN       20
#define NET_ZRL_BUTTON_DOWN_PIN     4
#define NET_ZRL_BUTTON_STOP_PIN     3

// LED-Pins fuer lokale Richtungs-/Kalibrier-Rueckmeldung.
#define NET_ZRL_LED_UP_PIN          7
#define NET_ZRL_LED_DOWN_PIN        6
#define NET_ZRL_LED_ACTIVE_HIGH     1

// Pegel-Logik der Taster. 0 bedeutet active-HIGH; gedrueckt = HIGH.
#define NET_ZRL_BUTTON_ACTIVE_LOW   0 // 0 = gedrueckt bei HIGH; wichtig fuer Pullup/Pulldown-Auswahl in setup().

// Fahrzeit-Fallback, bis reale Kalibrierwerte vorliegen. Damit sind nur
// Endlagenfahrten sinnvoll; Zwischenpositionen brauchen Kalibrierung.
#define NET_ZRL_DEFAULT_ESTIMATED_TRAVEL_TIME_MS 100000UL // 100 s Fallback; UL haelt die Rechnung unsigned long wie millis().
