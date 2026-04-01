#pragma once

#define SH_MASTER_PROFILE_PRIMARY    1U
#define SH_MASTER_PROFILE_SECONDARY  2U

#ifndef MASTER_PROFILE
#define MASTER_PROFILE SH_MASTER_PROFILE_PRIMARY
#endif

#if MASTER_PROFILE == SH_MASTER_PROFILE_PRIMARY
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

#ifndef MASTER_DEBUG_ENABLED
#define MASTER_DEBUG_ENABLED 1
#endif

#ifndef MASTER_WLAN_CHANNEL
#define MASTER_WLAN_CHANNEL 6
#endif

#ifndef MASTER_WIFI_RECONNECT_INTERVAL_MS
#define MASTER_WIFI_RECONNECT_INTERVAL_MS 10000UL
#endif

#ifndef MASTER_MQTT_RECONNECT_INTERVAL_MS
#define MASTER_MQTT_RECONNECT_INTERVAL_MS 5000UL
#endif

#ifndef MASTER_COMMAND_ACK_TIMEOUT_MS
#define MASTER_COMMAND_ACK_TIMEOUT_MS 800UL
#endif

#ifndef MASTER_COMMAND_MAX_RETRIES
#define MASTER_COMMAND_MAX_RETRIES 3U
#endif

#ifndef MASTER_COMMAND_ACK_CACHE_MS
#define MASTER_COMMAND_ACK_CACHE_MS 30000UL
#endif

#ifndef MASTER_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_NODE_OFFLINE_TIMEOUT_MS 15000UL
#endif

#ifndef MASTER_MQTT_BUFFER_BYTES
#define MASTER_MQTT_BUFFER_BYTES 768U
#endif

#ifndef MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS 900000UL
#endif

#ifndef MASTER_LOOP_INTERVAL_MS
#define MASTER_LOOP_INTERVAL_MS 10UL
#endif

// ============================================================
// Master - Geraetekonfiguration vor dem Upload
// ============================================================
// Diese Datei enthaelt alle Einstellungen, die vor dem Flashen
// geprueft und ggf. angepasst werden muessen.
//
// Private Zugangsdaten (WLAN, MQTT) stehen in Secrets.h,
// die nicht ins Repository gehoert (liegt in .gitignore).
// Vorlage: firmware/include/Secrets.example.h
// ============================================================

// Geraete-ID des Masters.
// Format: MASTER-001 (keine Sensorbestueckung in der ID)
constexpr char DEVICE_ID[]    = MASTER_PROFILE_DEVICE_ID;

// Anzeigename fuer MQTT-meta und Node-RED-Dashboard.
// Darf nach Inbetriebnahme ueber CFG geaendert werden.
constexpr char DEVICE_NAME[]  = MASTER_PROFILE_DEVICE_NAME;

// Firmware-Variante fuer serielle Diagnose und spaetere Profiltrennung.
constexpr char FW_VARIANT[]   = MASTER_PROFILE_FW_VARIANT;

// Debug-Stand: in fruehen Teststaenden aktiv lassen.
// Fuer Produktionseinsatz auf false setzen.
constexpr bool DEVICE_DEBUG_AKTIV = MASTER_DEBUG_ENABLED != 0;

// Fester WLAN-/ESP-NOW-Kanal.
// Muss mit dem Heimnetz-Kanal uebereinstimmen.
// Kanal 1, 6 oder 11 bevorzugen (keine Kanalueberlappung).
constexpr int WLAN_KANAL = MASTER_WLAN_CHANNEL;

// Reconnect-Takt fuer WLAN und MQTT.
constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = MASTER_WIFI_RECONNECT_INTERVAL_MS;
constexpr unsigned long MQTT_RECONNECT_INTERVAL_MS = MASTER_MQTT_RECONNECT_INTERVAL_MS;

// ACK-/Retry-Handling bleibt aktuell auf einen kleinen, kontrollierten
// Master-Slot begrenzt. Der Pfad ist damit noch keine generische
// Mehrgeraete-Orchestrierung, bildet aber den neutralen oberen
// Befehlskanal bereits ehrlicher ab.
constexpr unsigned long COMMAND_ACK_TIMEOUT_MS = MASTER_COMMAND_ACK_TIMEOUT_MS;
constexpr uint8_t COMMAND_MAX_RETRIES = MASTER_COMMAND_MAX_RETRIES;
constexpr unsigned long COMMAND_ACK_CACHE_MS = MASTER_COMMAND_ACK_CACHE_MS;

// Node gilt nach ausbleibenden Meldungen nicht fuer alle Basen gleich
// sofort als komplett offline. Besonders bei Batterieknoten wird oberhalb
// des Masters zusaetzlich zwischen erwartet schlafend, verspaetet und
// wirklich offline unterschieden.
constexpr unsigned long NODE_OFFLINE_TIMEOUT_MS = MASTER_NODE_OFFLINE_TIMEOUT_MS;
constexpr unsigned long BATTERY_NODE_OFFLINE_TIMEOUT_MS = MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS;
constexpr unsigned int MQTT_BUFFER_BYTES = MASTER_MQTT_BUFFER_BYTES;

// Takt der Hauptschleife.
constexpr unsigned long LOOP_INTERVAL_MS = MASTER_LOOP_INTERVAL_MS;
