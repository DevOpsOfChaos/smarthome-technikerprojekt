// =============================================================================
// DeviceConfig.h – Geraetekonfiguration NET-ERL Kitchen
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_kitchen/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       NET-ERL-002
//   Name:     NET-ERL Kitchen
//   Variante: net_erl_kitchen
//   Caps:     RELAY | TEMP | HUM | LUX | MOTION | AQI | PRESSURE | BUTTON | LED_RING
//
// Sensoren/Aktoren:
//   BME680:     I2C, 0x76/0x77 (Temp/Feuchte/Druck/Gas)
//   VEML7700:   I2C, 0x10 (Lux)
//   ENS160:     I2C, 0x52/0x53 (AQI/TVOC/eCO2)
//   LD2410C:    GPIO7 (Radar-Praesenz)
//   NeoPixel:   GPIO8, 17 LEDs
//   Button:     GPIO6 (active-LOW, 40ms Debounce)
//   Relais:     GPIO10 (active-HIGH)
//
// Auto-Licht: wie Hall-Light (PIR durch LD2410 ersetzt)
// STATE: ExtendedRelayComfortGasConfigStateReportPayload (volle Sensorik + AQI)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_ERL_DEVICE_ID "NET-ERL-002"
#define NET_ERL_DEVICE_NAME "NET-ERL Kitchen"
#define NET_ERL_FW_VARIANT "net_erl_kitchen"
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY | SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_MOTION | SH_CAP_AQI | SH_CAP_PRESSURE | SH_CAP_BUTTON | SH_CAP_LED_RING)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_KITCHEN_LIGHT
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

#define NET_ERL_DEBUG_ENABLED 1
#define NET_ERL_WLAN_CHANNEL 6

#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL
#define NET_ERL_LOOP_INTERVAL_MS 20UL

#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U
#define NET_ERL_BOOT_COUNTER 1U

#define NET_ERL_DEFAULT_REPORT_INTERVAL_S 10U
#define NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD 250U
#define NET_ERL_DEFAULT_AUTO_OFF_DELAY_S 15U

#define NET_ERL_SENSOR_POLL_INTERVAL_MS 50UL        // LD2410-Poll
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS 2500UL       // BME680/ENS160
#define NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS 30000UL
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 30000UL

#define NET_ERL_I2C_CLOCK_HZ 10000UL

// BME680
#define NET_ERL_BME680_PRIMARY_ADDRESS 0x76
#define NET_ERL_BME680_FALLBACK_ADDRESS 0x77
#define NET_ERL_BME680_GAS_WARMUP_MS 180000UL      // 3min bis gas_ohm belastbar
#define NET_ERL_BME680_GAS_WARMUP_MIN_READS 5U

// ENS160
#define NET_ERL_ENS160_PRIMARY_ADDRESS 0x52
#define NET_ERL_ENS160_FALLBACK_ADDRESS 0x53
#define NET_ERL_ENS160_WARMUP_MS 180000UL
#define NET_ERL_ENS160_STALE_TIMEOUT_MS 120000UL   // 2min ohne Daten = stale

// Button
#define BUTTON_DEBOUNCE_MS 40UL
