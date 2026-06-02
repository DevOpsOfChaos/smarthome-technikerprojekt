/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: BAT-SEN Window Contact Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer den batteriebetriebenen Fensterkontakt
 Beschreibung: Definiert Identitaet, Faehigkeiten, Batterieprofil, Wake-Verhalten
 und Kontaktparameter fuer bat_sen_window_contact. Die Runtime liest diese
 Defines zur Compile-Zeit ein.

 Genutzte Bibliotheken:
 - DeviceTypes.h: eigene Protokollbibliothek mit Capability-Flags und Batterieprofilen.

 Wichtige Werte:
 - 43200 Sekunden Wake-Intervall entsprechen 12 Stunden.
 - 5000 Millisekunden RX-Fenster entsprechen 5 Sekunden Empfangszeit nach Wake.
 - 35 Millisekunden Entprellzeit verhindern falsche Kontaktwechsel.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer BAT-SEN Window Contact angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

#pragma once

// DeviceTypes.h liefert die SH_CAP_*-Flags, Batterieprofile und Reporting-Modi.
// Diese Datei definiert nur Makros; sie wird von BatSenRuntime.h zur Compile-Zeit
// ausgewertet und erzeugt keine eigenen Funktionen oder Objekte.
#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Identitaet, die im HELLO-Payload zum Master geht und spaeter in MQTT-Meta
// sichtbar wird. device_id muss stabil bleiben, sonst behandelt der Master das
// gleiche Board als neue Node.
#define BAT_SEN_DEVICE_ID "bat_sen_010"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Window"
#define BAT_SEN_FW_VARIANT "bat_sen_window_contact"
// Capabilities beschreiben die fachlichen Werte dieses Nodes:
// Batterieinformationen plus Fensterkontakt. Daraus leitet der Master ab,
// welche MQTT-State-Felder gueltig sind.
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_WINDOW)

// Bring-up-MAC fuer die Migration auf die neue dynamische Master-Registry.
// Lokal administrierte Unicast-MAC; entfernen oder aendern, wenn dieses Board
// dauerhaft mit seiner echten Hardware-MAC laufen soll.
#define BAT_SEN_WIFI_STA_MAC_OVERRIDE {0x02, 0xBA, 0x75, 0xE0, 0x10, 0x01}

// BAT_SEN-Devices verwenden BAT_SEN_*-Praefix ohne _DEVICE_-Infix
// (anders als NET_ERL_DEVICE_* und NET_SEN_DEVICE_*).
#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// Batterie: 2x AAA in Reihe. Das Profil bestimmt die Prozentrechnung aus mV.
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_2X_AAA
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 43200U   // 43200 Sekunden = 12 Stunden.
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U       // 5000 Millisekunden = 5 Sekunden.

// GPIO-Wake aktiv: wird bei Pegelwechsel am Kontakt-Pin aufgeweckt
#define BAT_SEN_ENABLE_GPIO_WAKE 1
// Kurzer Setup-Tasterdruck sperrt/erlaubt Deep-Sleep fuer Tests am Geraet.
#define BAT_SEN_ENABLE_STAY_AWAKE_TOGGLE 1
// Bring-up-Diagnose: GPIO7 blinkt beim Boot dreimal kurz.
#define BAT_SEN_ENABLE_SETUP_LED_BOOT_TEST 1
// Fallback-Wake-Level; die Fensterkontakt-Firmware nutzt dynamischen Wake-Level.
#define BAT_SEN_GPIO_WAKE_LEVEL_HIGH BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH

// Kontakt-Parameter:
// OPEN_LEVEL_HIGH koppelt die fachliche Bedeutung "Fenster offen" an den
// elektrischen Pegel. Aenderung hier muss zur realen Reed-/Magnet-Verdrahtung
// passen, sonst werden Events invertiert.
#define BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS 35UL      // 35 Millisekunden Entprellzeit.
#define BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP 1    // Pullup aktiv
#define BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH 1     // offen = HIGH
