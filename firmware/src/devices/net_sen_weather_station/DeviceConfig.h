// =============================================================================
// DeviceConfig.h – Geraetekonfiguration NET-SEN Env BME280+VEML+Rain
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_weather_station/DeviceConfig.h
//
// === EINSATZZWECK ===
// Netzbetriebene Regenstation fuer Aussenumgebung:
// Temperatur, Feuchte, Luftdruck, Helligkeit und digitaler Nass/Trocken-Status.
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       NET-SEN-002
//   Name:     NET-SEN Env BME280+VEML+Rain
//   Variante: net_sen_weather_station
//   Caps:     TEMP | HUM | LUX | PRESSURE | RAIN
//
// Sensoren:
//   BME280:      I2C, Adresse 0x76/0x77
//   VEML7700:    I2C, Adresse 0x10
//   Regensensor: GPIO3, digital, active-LOW mit Pullup
//
// Regen wird als Event gemeldet. Der Server leitet daraus den aktuellen rain-State ab.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-16
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_SEN_DEVICE_ID "NET-SEN-002"
#define NET_SEN_DEVICE_NAME "NET-SEN Env BME280+VEML+Rain"
#define NET_SEN_FW_VARIANT "net_sen_weather_station"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_PRESSURE | SH_CAP_RAIN)
#define NET_SEN_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

#define NET_SEN_ENABLE_I2C_BASE 1

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_SEN_HEARTBEAT_INTERVAL_MS 20000UL
#define NET_SEN_DEFAULT_REPORT_INTERVAL_S 10U
#define NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S 10U
#define NET_SEN_STATE_INTERVAL_MS (NET_SEN_DEFAULT_REPORT_INTERVAL_S * 1000UL)
#define NET_SEN_MIN_REPORT_INTERVAL_S 5U
#define NET_SEN_MAX_REPORT_INTERVAL_S 600U
#define NET_SEN_LOOP_INTERVAL_MS 50UL

// Sensor-Timing
#define NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS 2500UL
#define NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS 15000UL
#define NET_SEN_ENV_BME280_VEML_RAIN_SNAPSHOT_LOG_INTERVAL_MS 30000UL

// Hysterese
#define NET_SEN_ENV_BME280_VEML_RAIN_TEMP_DELTA_01C 10
#define NET_SEN_ENV_BME280_VEML_RAIN_HUM_DELTA_01PCT 50U
#define NET_SEN_ENV_BME280_VEML_RAIN_LUX_DELTA 25U
#define NET_SEN_ENV_BME280_VEML_RAIN_PRESSURE_DELTA_PA 30UL

// I2C-Adressen
#define NET_SEN_ENV_BME280_PRIMARY_ADDRESS 0x76
#define NET_SEN_ENV_BME280_FALLBACK_ADDRESS 0x77
#define NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS 1050UL
