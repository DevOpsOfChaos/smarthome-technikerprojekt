// =============================================================================
// DeviceConfig.h – Geraetekonfiguration NET-SEN Env BME680+VEML+ENS160
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_env_bme680_veml/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       net_sen_01
//   Name:     NET-SEN Env BME680+VEML+ENS160
//   Variante: net_sen_env_bme680_veml
//   Caps:     TEMP | HUM | LUX | PRESSURE | AQI
//
// Sensoren:
//   BME680:     I2C, Temp/Feuchte/Druck/Rohgas (Adresse 0x76/0x77)
//   VEML7700:   I2C, Lux (Adresse 0x10)
//   ENS160:     I2C, AQI/TVOC/eCO2 (Adresse 0x52/0x53)
//
// Gas-Warmup: 180s, min. 5 gueltige Messungen vor gas_ohm belastbar
// ENS160-Kompensation via BME680-Temperatur/Feuchte
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_SEN_DEVICE_ID "net_sen_01"
#define NET_SEN_DEVICE_NAME "NET-SEN Env BME680+VEML+ENS160"
#define NET_SEN_FW_VARIANT "net_sen_env_bme680_veml"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_PRESSURE | SH_CAP_AQI)

#define NET_SEN_ENABLE_I2C_BASE 1

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_SEN_HEARTBEAT_INTERVAL_MS 60000UL
#define NET_SEN_STATE_INTERVAL_MS 60000UL

// Sensor-Timing
#define NET_SEN_ENV_BME680_VEML_SENSOR_READ_INTERVAL_MS 2500UL
#define NET_SEN_ENV_BME680_VEML_ERROR_LOG_INTERVAL_MS 15000UL

// Hysterese (Schwellwerte fuer STATE-Aenderung)
#define NET_SEN_ENV_BME680_VEML_TEMP_DELTA_01C 10
#define NET_SEN_ENV_BME680_VEML_HUM_DELTA_01PCT 50U
#define NET_SEN_ENV_BME680_VEML_LUX_DELTA 25U
#define NET_SEN_ENV_BME680_VEML_PRESSURE_DELTA_PA 30UL
#define NET_SEN_ENV_BME680_VEML_GAS_DELTA_OHM 500UL
#define NET_SEN_ENV_BME680_VEML_AQI_DELTA 10U
#define NET_SEN_ENV_BME680_VEML_TVOC_DELTA_PPB 20U
#define NET_SEN_ENV_BME680_VEML_ECO2_DELTA_PPM 25U

// BME680
#define NET_SEN_ENV_BME680_PRIMARY_ADDRESS 0x76
#define NET_SEN_ENV_BME680_FALLBACK_ADDRESS 0x77
#define NET_SEN_ENV_BME680_GAS_WARMUP_MS 180000UL
#define NET_SEN_ENV_BME680_GAS_WARMUP_MIN_READS 5U

// ENS160
#define NET_SEN_ENV_ENS160_PRIMARY_ADDRESS 0x52
#define NET_SEN_ENV_ENS160_FALLBACK_ADDRESS 0x53
#define NET_SEN_ENV_ENS160_WARMUP_MS 180000UL
#define NET_SEN_ENV_ENS160_STALE_TIMEOUT_MS 120000UL

// VEML7700
#define NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS 1050UL
