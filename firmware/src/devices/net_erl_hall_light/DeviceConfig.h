// =============================================================================
// DeviceConfig.h – Geraetekonfiguration NET-ERL Hall Light
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_hall_light/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       NET-ERL-001
//   Name:     NET-ERL Hall Light
//   Variante: net_erl_hall_light
//   Caps:     RELAY | TEMP | HUM | LUX | MOTION
//
// Sensoren:
//   BME280:   I2C, 0x76 (Temp/Feuchte – Druck bleibt aussen vor)
//   VEML7700: I2C, 0x10 (Lux)
//   PIR:      GPIO6 (Praesenz)
//
// Auto-Licht-Parameter:
//   Lux-Schwelle:     250 Lux (einschalten nur unter diesem Wert)
//   Auto-Off-Delay:   15s (nach letzter Bewegung)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_ERL_DEVICE_ID "NET-ERL-001"
#define NET_ERL_DEVICE_NAME "NET-ERL Hall Light"
#define NET_ERL_FW_VARIANT "net_erl_hall_light"

// Druck bleibt trotz BME280 bewusst aussen vor (Aussenvertrag ohne PRESSURE)
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY | SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_MOTION)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_HALL_LIGHT
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

#define NET_ERL_DEBUG_ENABLED 0
#define NET_ERL_WLAN_CHANNEL 6

#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL
#define NET_ERL_LOOP_INTERVAL_MS 20UL

#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U
#define NET_ERL_BOOT_COUNTER 1U

#define NET_ERL_DEFAULT_REPORT_INTERVAL_S 10U
#define NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD 250U  // Lux unter diesem Wert = einschalten
#define NET_ERL_DEFAULT_AUTO_OFF_DELAY_S 15U        // Nachlaufzeit (s)

#define NET_ERL_SENSOR_POLL_INTERVAL_MS 250UL       // PIR-Poll-Intervall
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS 2000UL       // BME280-Lese-Intervall
#define NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS 30000UL
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 30000UL

#define NET_ERL_BME280_ADDRESS 0x76
