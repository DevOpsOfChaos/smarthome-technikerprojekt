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
#define MASTER_COMMAND_MAX_RETRIES 2U
#endif

#ifndef MASTER_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_NODE_OFFLINE_TIMEOUT_MS 75000UL
#endif

#ifndef MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS
#define MASTER_BATTERY_NODE_OFFLINE_TIMEOUT_MS 600000UL
#endif

#ifndef MASTER_MAX_DYNAMIC_NODES
#define MASTER_MAX_DYNAMIC_NODES 16U
#endif

#ifndef MASTER_MQTT_BUFFER_BYTES
#define MASTER_MQTT_BUFFER_BYTES 768U
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
