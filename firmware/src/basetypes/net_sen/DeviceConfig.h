// =============================================================================
// DeviceConfig.h – Geraetekonfiguration fuer NET-SEN Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_sen/DeviceConfig.h
//
// Datei-Funktion:
//   Default-Konfiguration fuer den NET-SEN-Basis-Sensor. Definiert
//   Geraete-Identitaet, Faehigkeiten (Caps), I2C-Aktivierung,
//   Timing-Parameter und Report-Intervalle. Alle Werte koennen
//   von konkreten Device-Konfigurationen ueberschrieben werden (#ifndef).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include <stdint.h>

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// =============================================================================
// GERAETE-IDENTITAET – ID, Name, Variante (pro Device ueberschreibbar)
// =============================================================================

#ifndef NET_SEN_DEVICE_ID
#define NET_SEN_DEVICE_ID "net_sen_base_01"
#endif

#ifndef NET_SEN_DEVICE_NAME
#define NET_SEN_DEVICE_NAME "NET-SEN Base"
#endif

#ifndef NET_SEN_FW_VARIANT
#define NET_SEN_FW_VARIANT "net_sen_base"
#endif

// =============================================================================
// GERAETE-EIGENSCHAFTEN – Caps, Debug, WLAN
// =============================================================================

#ifndef NET_SEN_DEVICE_CAPS
#define NET_SEN_DEVICE_CAPS 0U              // Faehigkeiten (Bitmaske, 0=keine Sensoren)
#endif

#ifndef NET_SEN_DEBUG_ENABLED
#define NET_SEN_DEBUG_ENABLED 1             // Debug-Ausgaben ein/aus
#endif

#ifndef NET_SEN_WLAN_CHANNEL
#define NET_SEN_WLAN_CHANNEL 6              // ESP-NOW WLAN-Kanal
#endif

// =============================================================================
// TIMING – Intervalle fuer HELLO, HEARTBEAT, STATE, LOOP
// =============================================================================

#ifndef NET_SEN_HELLO_RETRY_INTERVAL_MS
#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL  // HELLO-Wiederholung (ms)
#endif

#ifndef NET_SEN_HEARTBEAT_INTERVAL_MS
#define NET_SEN_HEARTBEAT_INTERVAL_MS 60000UL   // Heartbeat-Intervall (ms, 60s)
#endif

#ifndef NET_SEN_STATE_INTERVAL_MS
#define NET_SEN_STATE_INTERVAL_MS 60000UL       // Default STATE-Intervall (ms, 60s)
#endif

#ifndef NET_SEN_DEFAULT_REPORT_INTERVAL_S
#define NET_SEN_DEFAULT_REPORT_INTERVAL_S (NET_SEN_STATE_INTERVAL_MS / 1000UL)
#endif

#ifndef NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S
#define NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S NET_SEN_DEFAULT_REPORT_INTERVAL_S
#endif

#ifndef NET_SEN_LOOP_INTERVAL_MS
#define NET_SEN_LOOP_INTERVAL_MS 50UL           // Loop-Ausfuehrungsintervall (ms)
#endif

// =============================================================================
// I2C – I2C-Bus-Aktivierung (fuer I2C-Sensoren wie BME280)
// =============================================================================

#ifndef NET_SEN_ENABLE_I2C_BASE
#define NET_SEN_ENABLE_I2C_BASE 0               // 1 = I2C-Bus initialisieren
#endif

// =============================================================================
// REPORT-INTERVALLE – Min/Max (fuer Provisioning)
// =============================================================================

#ifndef NET_SEN_DEVICE_REPORTING_MODE
#define NET_SEN_DEVICE_REPORTING_MODE SH_REPORTING_PERIODIC  // Periodischer Report
#endif

#ifndef NET_SEN_MIN_REPORT_INTERVAL_S
#define NET_SEN_MIN_REPORT_INTERVAL_S 10U                   // Minimal 10s
#endif

#ifndef NET_SEN_MAX_REPORT_INTERVAL_S
#define NET_SEN_MAX_REPORT_INTERVAL_S 3600U                 // Maximal 3600s (1h)
#endif

// =============================================================================
// ABGELEITETE KONSTANTEN – Aus #defines berechnete constexpr-Werte
// =============================================================================

constexpr char DEVICE_ID[] = NET_SEN_DEVICE_ID;
constexpr char DEVICE_NAME[] = NET_SEN_DEVICE_NAME;
constexpr char FW_VARIANT[] = NET_SEN_FW_VARIANT;
constexpr bool DEVICE_DEBUG_AKTIV = NET_SEN_DEBUG_ENABLED != 0;
constexpr uint16_t DEVICE_CAPS = (uint16_t)NET_SEN_DEVICE_CAPS;
constexpr bool I2C_BASIS_AKTIV = NET_SEN_ENABLE_I2C_BASE != 0;

constexpr uint8_t DEVICE_META_SCHEMA_VERSION = SH_META_SCHEMA_VERSION_CURRENT;
constexpr uint8_t DEVICE_CONTROL_MODE = SH_CONTROL_MODE_NONE;
constexpr uint8_t DEVICE_CONFIG_PROFILE = SH_PROFILE_NONE;
constexpr uint8_t DEVICE_REPORTING_MODE = NET_SEN_DEVICE_REPORTING_MODE;

constexpr unsigned long HELLO_RETRY_INTERVAL_MS = NET_SEN_HELLO_RETRY_INTERVAL_MS;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = NET_SEN_HEARTBEAT_INTERVAL_MS;
constexpr unsigned long STATE_INTERVAL_MS = NET_SEN_STATE_INTERVAL_MS;
constexpr uint32_t DEFAULT_REPORT_INTERVAL_S = NET_SEN_DEFAULT_REPORT_INTERVAL_S;
constexpr uint32_t DEFAULT_SENSOR_SEND_INTERVAL_S = NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S;
constexpr unsigned long LOOP_INTERVAL_MS = NET_SEN_LOOP_INTERVAL_MS;
constexpr uint32_t MIN_REPORT_INTERVAL_S = NET_SEN_MIN_REPORT_INTERVAL_S;
constexpr uint32_t MAX_REPORT_INTERVAL_S = NET_SEN_MAX_REPORT_INTERVAL_S;
constexpr int WLAN_KANAL = NET_SEN_WLAN_CHANNEL;
