/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: NET-SEN Weather Station Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Netzbetriebener Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer die netzbetriebene Wetterstation
 Beschreibung: Definiert Identitaet, Faehigkeiten, Meldeintervalle, Sensorzeiten,
 Delta-Schwellen und I2C-Adressen fuer net_sen_weather_station. Regen wird als
 Event gemeldet; der Master leitet daraus den aktuellen Regenstatus ab.

 Genutzte Bibliotheken:
 - DeviceTypes.h: eigene Protokollbibliothek mit Capability-Flags und Reporting-Modi.

 Wichtige Werte:
 - 5000 Millisekunden HELLO-Retry entsprechen 5 Sekunden.
 - 20000 Millisekunden Heartbeat entsprechen 20 Sekunden.
 - 60000 Millisekunden Sensor-Read-Intervall entsprechen 60 Sekunden.
 - 15000 Millisekunden Fehlerlog-Intervall entsprechen 15 Sekunden.
 - 30000 Millisekunden Snapshot-Intervall entsprechen 30 Sekunden.
 - 1050 Millisekunden VEML7700-Startverzoegerung entsprechen 1,05 Sekunden.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer NET-SEN Weather Station angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht.
===============================================================================
*/

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_SEN_DEVICE_ID "NET-SEN-002"
#define NET_SEN_DEVICE_NAME "NET-SEN Env BME280+VEML+Rain"
#define NET_SEN_FW_VARIANT "net_sen_weather_station"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_PRESSURE | SH_CAP_RAIN)
#define NET_SEN_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

// Reiner Sensor-Node: Kein CONTROL_MODE (none) und kein CONFIG_PROFILE (none).
// Diese Defines werden bewusst nicht gesetzt; die Runtime verwendet Default-Werte.

#define NET_SEN_ENABLE_I2C_BASE 1

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL  // 5000 Millisekunden = 5 Sekunden.
#define NET_SEN_HEARTBEAT_INTERVAL_MS 20000UL   // 20000 Millisekunden = 20 Sekunden.
#define NET_SEN_DEFAULT_REPORT_INTERVAL_S 10U
// NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S ist identisch mit DEFAULT_REPORT_INTERVAL_S.
// Beide bleiben fuer Abwaertskompatibilitaet erhalten.
#define NET_SEN_DEFAULT_SENSOR_SEND_INTERVAL_S 10U
#define NET_SEN_STATE_INTERVAL_MS (NET_SEN_DEFAULT_REPORT_INTERVAL_S * 1000UL)
#define NET_SEN_MIN_REPORT_INTERVAL_S 5U
#define NET_SEN_MAX_REPORT_INTERVAL_S 600U
#define NET_SEN_LOOP_INTERVAL_MS 50UL           // 50 Millisekunden Loop-Pause.

// Sensor-Timing
#define NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS 60000UL // 60000 Millisekunden = 60 Sekunden.
#define NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS 15000UL   // 15000 Millisekunden = 15 Sekunden.
#define NET_SEN_ENV_BME280_VEML_RAIN_SNAPSHOT_LOG_INTERVAL_MS 30000UL // 30000 Millisekunden = 30 Sekunden.

// Hysterese
#define NET_SEN_ENV_BME280_VEML_RAIN_TEMP_DELTA_01C 10
#define NET_SEN_ENV_BME280_VEML_RAIN_HUM_DELTA_01PCT 50U
#define NET_SEN_ENV_BME280_VEML_RAIN_LUX_DELTA 25U
#define NET_SEN_ENV_BME280_VEML_RAIN_PRESSURE_DELTA_PA 30UL

// I2C-Adressen
#define NET_SEN_ENV_BME280_PRIMARY_ADDRESS 0x76
#define NET_SEN_ENV_BME280_FALLBACK_ADDRESS 0x77
#define NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS 1050UL // 1050 Millisekunden = 1,05 Sekunden.
