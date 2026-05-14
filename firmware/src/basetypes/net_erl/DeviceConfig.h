// =============================================================================
// DeviceConfig.h – Geraetekonfiguration fuer NET-ERL Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_erl/DeviceConfig.h
//
// Datei-Funktion:
//   Default-Konfiguration fuer das NET-ERL-Basisgeraet. Definiert
//   Geraete-Identitaet (ID, Name, Variante), Faehigkeiten (Caps),
//   Timing-Parameter und Report-Intervalle. Alle Werte koennen
//   von konkreten Device-Konfigurationen ueberschrieben werden (#ifndef).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Änderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// =============================================================================
// GERAETE-IDENTITAET – ID, Name, Variante (pro Device ueberschreibbar)
// =============================================================================

// device_id als String (max. 32 Zeichen, wird auch als Setup-AP-SSID genutzt)
#ifndef NET_ERL_DEVICE_ID
#define NET_ERL_DEVICE_ID "net_erl_01"
#endif

// Anzeigename in Logs und Provisioning
#ifndef NET_ERL_DEVICE_NAME
#define NET_ERL_DEVICE_NAME "NET-ERL Blank Relay Test"
#endif

// Firmware-Variante (z.B. "net_erl_base", "net_erl_hall_light")
#ifndef NET_ERL_FW_VARIANT
#define NET_ERL_FW_VARIANT "net_erl_base"
#endif

// =============================================================================
// GERAETE-EIGENSCHAFTEN – Faehigkeiten, Modi, Profile
// =============================================================================

// Faehigkeiten als Bitmaske (Bit 0 = Relais = SH_CAP_RELAY)
#ifndef NET_ERL_DEVICE_CAPS
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY)
#endif

// Steuerungsmodus: SH_CONTROL_MODE_RELAY (einfaches Ein/Aus-Relais)
#ifndef NET_ERL_DEVICE_CONTROL_MODE
#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY
#endif

// Konfigurationsprofil: SH_PROFILE_NONE (keine Sonderlogik)
#ifndef NET_ERL_DEVICE_CONFIG_PROFILE
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_NONE
#endif

// Report-Modus: SH_REPORTING_HYBRID (regelmaessiger Report + Event-basiert)
#ifndef NET_ERL_DEVICE_REPORTING_MODE
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID
#endif

// =============================================================================
// DEBUG – Serielle Ausgaben
// =============================================================================

// Debug-Ausgaben einschalten (1) oder deaktivieren (0)
// Wird mit globalem DEBUG_AKTIV aus DebugConfig.h UND-verknuepft
#ifndef NET_ERL_DEBUG_ENABLED
#define NET_ERL_DEBUG_ENABLED 1
#endif

// =============================================================================
// FUNK – WLAN-Kanal und ESP-NOW
// =============================================================================

// ESP-NOW WLAN-Kanal (muss mit Master-Kanal uebereinstimmen)
#ifndef NET_ERL_WLAN_CHANNEL
#define NET_ERL_WLAN_CHANNEL 6
#endif

// =============================================================================
// TIMING – Intervalle fuer HELLO, HEARTBEAT, STATE, LOOP
// =============================================================================

// Wiederholungsintervall fuer HELLO-Nachrichten bei Master-Suche (ms)
#ifndef NET_ERL_HELLO_RETRY_INTERVAL_MS
#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#endif

// Heartbeat-Intervall: wie oft das Geraet "noch da"-Signal sendet (ms)
#ifndef NET_ERL_HEARTBEAT_INTERVAL_MS
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL
#endif

// Standard-Intervall fuer STATE-Report (ms) – wird in report_interval_s umgerechnet
#ifndef NET_ERL_STATE_INTERVAL_MS
#define NET_ERL_STATE_INTERVAL_MS 10000UL
#endif

// Loop-Ausfuehrungsintervall (ms) – wie oft loop() durchlaeuft
#ifndef NET_ERL_LOOP_INTERVAL_MS
#define NET_ERL_LOOP_INTERVAL_MS 20UL
#endif

// =============================================================================
// REPORT-INTERVALLE – Minimal und Maximal (fuer Provisioning)
// =============================================================================

// Minimal zulaessiges Report-Intervall (Sekunden)
#ifndef NET_ERL_MIN_REPORT_INTERVAL_S
#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#endif

// Maximal zulaessiges Report-Intervall (Sekunden, 600s = 10 Minuten)
#ifndef NET_ERL_MAX_REPORT_INTERVAL_S
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U
#endif

// =============================================================================
// BOOT – Boot-Zaehler (wird bei Neustart inkrementiert)
// =============================================================================

// Boot-Zaehler: wird bei jedem HELLO mitgesendet (Master erkennt Neustarts)
#ifndef NET_ERL_BOOT_COUNTER
#define NET_ERL_BOOT_COUNTER 1U
#endif
