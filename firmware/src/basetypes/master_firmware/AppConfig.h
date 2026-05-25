/*
===============================================================================
 Datei: AppConfig.h
 Code-Name: Master Firmware Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Basistyp-Konfiguration / ESP-NOW-MQTT-Master
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Konfiguration fuer die Master-Firmware
 Beschreibung: Definiert Master-Profile, Geraeteidentitaet, WLAN-/MQTT-Timer,
 ACK-/Retry-Grenzen, Node-Limits, MQTT-Puffergroesse und daraus abgeleitete
 Konstanten. Zugangsdaten werden nicht hier abgelegt, sondern ueber Secrets.h
 ausserhalb der oeffentlichen technischen Projektlinie eingebunden.

 Wichtige Werte:
 - 10000 Millisekunden WLAN-Reconnect-Intervall entsprechen 10 Sekunden.
 - 5000 Millisekunden MQTT-Reconnect-Intervall entsprechen 5 Sekunden.
 - 800 Millisekunden ACK-Timeout vor einem Retry.
 - 2 Retries bedeuten maximal 3 Sendeversuche pro bestaetigungspflichtigem Befehl.
 - 75000 Millisekunden Netz-Node-Timeout entsprechen 75 Sekunden.
 - 600000 Millisekunden Batterie-Node-Timeout entsprechen 10 Minuten.
 - 16 dynamische Nodes sind maximal gleichzeitig in der Registry vorgesehen.

 Aenderungsverlauf:
 - 2026-05-14: Master-Konfiguration angelegt.
 - 2026-05-18: Dateiheader und Kommentare an Referenzstil angepasst.
===============================================================================
*/

#pragma once

// =============================================================================
// PROFIL-SYSTEM - Geraete-Identitaet je nach Profil (Primary/Secondary)
// =============================================================================

#define SH_MASTER_PROFILE_PRIMARY    1U
#define SH_MASTER_PROFILE_SECONDARY  2U

#ifndef MASTER_PROFILE
#define MASTER_PROFILE SH_MASTER_PROFILE_PRIMARY  // Default: Primary
#endif

#if MASTER_PROFILE == SH_MASTER_PROFILE_PRIMARY
  // Primary Master (Standard-Geraet)
  #ifndef MASTER_PROFILE_DEVICE_ID
    #define MASTER_PROFILE_DEVICE_ID "MASTER-001"
  #endif
  #ifndef MASTER_PROFILE_DEVICE_NAME
    #define MASTER_PROFILE_DEVICE_NAME "Master"
  #endif
  #ifndef MASTER_PROFILE_FW_VARIANT
    #define MASTER_PROFILE_FW_VARIANT "master"
  #endif
#elif MASTER_PROFILE == SH_MASTER_PROFILE_SECONDARY
  // Secondary Master (Reserve-Geraet)
  #ifndef MASTER_PROFILE_DEVICE_ID
    #define MASTER_PROFILE_DEVICE_ID "MASTER-002"
  #endif
  #ifndef MASTER_PROFILE_DEVICE_NAME
    #define MASTER_PROFILE_DEVICE_NAME "Master 2"
  #endif
  #ifndef MASTER_PROFILE_FW_VARIANT
    #define MASTER_PROFILE_FW_VARIANT "master_secondary"
  #endif
#else
  #error "Unbekanntes MASTER_PROFILE."
#endif

// =============================================================================
// DEBUG - Serielle Ausgaben
// =============================================================================

#ifndef MASTER_DEBUG_ENABLED
#define MASTER_DEBUG_ENABLED 1U
#endif

// =============================================================================
// FUNK / WLAN - Kanal und Reconnect
// =============================================================================

// ESP-NOW WLAN-Kanal (muss mit Nodes uebereinstimmen)
#ifndef MASTER_WLAN_CHANNEL
#define MASTER_WLAN_CHANNEL 6
#endif

// WLAN-Reconnect-Intervall in Millisekunden; wie oft WLAN erneut verbunden wird.
#ifndef MASTER_WIFI_RECONNECT_INTERVAL_MS
#define MASTER_WIFI_RECONNECT_INTERVAL_MS 10000UL
#endif

// MQTT-Reconnect-Intervall in Millisekunden; wie oft MQTT erneut verbunden wird.
#ifndef MASTER_MQTT_RECONNECT_INTERVAL_MS
#define MASTER_MQTT_RECONNECT_INTERVAL_MS 5000UL
#endif

// =============================================================================
// ACK / PENDING - Timeout und Retries fuer Befehle an Nodes
// =============================================================================

// Timeout bis zum ersten Retry (ms)
#ifndef MASTER_COMMAND_ACK_TIMEOUT_MS
#define MASTER_COMMAND_ACK_TIMEOUT_MS 800UL
#endif

// Maximale Anzahl Retries bevor "timeout" gemeldet wird
#ifndef MASTER_COMMAND_MAX_RETRIES
#define MASTER_COMMAND_MAX_RETRIES 2U
#endif

// =============================================================================
// NODE-MANAGEMENT - Registry-Groesse und Offline-Timeout
// =============================================================================

// Offline-Timeout fuer Netz-Nodes (ms, 75s = 3-4 verpasste Heartbeats)
#ifndef MASTER_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_NODE_OFFLINE_TIMEOUT_MS 75000UL
#endif

// Offline-Timeout fuer Batterie-Nodes (ms, 10min)
#ifndef MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS 600000UL
#endif

// Maximale Anzahl dynamischer Nodes in der Registry
#ifndef MASTER_MAX_DYNAMIC_NODES
#define MASTER_MAX_DYNAMIC_NODES 16U
#endif

// =============================================================================
// MQTT - Buffer-Groesse und Loop-Intervall
// =============================================================================

// MQTT-Puffergroesse (B) fuer eingehende Nachrichten
#ifndef MASTER_MQTT_BUFFER_BYTES
#define MASTER_MQTT_BUFFER_BYTES 1024U
#endif

// Loop-Ausfuehrungsintervall (ms)
#ifndef MASTER_LOOP_INTERVAL_MS
#define MASTER_LOOP_INTERVAL_MS 10UL
#endif

// =============================================================================
// HINWEIS - Zugangsdaten (WLAN, MQTT) in Secrets.h
// =============================================================================
// Private Zugangsdaten stehen in Secrets.h (ausserhalb Repo, siehe .gitignore).
// Vorlage: firmware/include/Secrets.example.h
// =============================================================================

// =============================================================================
// ABGELEITETE KONSTANTEN - Aus #defines als constexpr
// =============================================================================

constexpr char DEVICE_ID[]    = MASTER_PROFILE_DEVICE_ID;
constexpr char DEVICE_NAME[]  = MASTER_PROFILE_DEVICE_NAME;
constexpr char FW_VARIANT[]   = MASTER_PROFILE_FW_VARIANT;
constexpr bool DEVICE_DEBUG_AKTIV = MASTER_DEBUG_ENABLED != 0;
constexpr int WLAN_KANAL = MASTER_WLAN_CHANNEL;
constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = MASTER_WIFI_RECONNECT_INTERVAL_MS;
constexpr unsigned long MQTT_RECONNECT_INTERVAL_MS = MASTER_MQTT_RECONNECT_INTERVAL_MS;
constexpr unsigned long COMMAND_ACK_TIMEOUT_MS = MASTER_COMMAND_ACK_TIMEOUT_MS;
constexpr unsigned int COMMAND_MAX_RETRIES = MASTER_COMMAND_MAX_RETRIES;
constexpr unsigned long NODE_OFFLINE_TIMEOUT_MS = MASTER_NODE_OFFLINE_TIMEOUT_MS;
constexpr unsigned long BATTERY_NODE_OFFLINE_TIMEOUT_MS = MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS;
constexpr unsigned int MAX_DYNAMIC_NODES = MASTER_MAX_DYNAMIC_NODES;
constexpr unsigned int MQTT_BUFFER_BYTES = MASTER_MQTT_BUFFER_BYTES;
constexpr unsigned long LOOP_INTERVAL_MS = MASTER_LOOP_INTERVAL_MS;
