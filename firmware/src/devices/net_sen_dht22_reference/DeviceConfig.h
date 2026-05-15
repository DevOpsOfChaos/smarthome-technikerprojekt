// =============================================================================
// DeviceConfig.h – Geraetekonfiguration NET-SEN DHT22 Reference
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_dht22_reference/DeviceConfig.h
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN: Referenz-Temperatur/Feuchte-Sensor, z.B. Keller/Aussen]
// === EINSATZZWECK ===
//
// Geraete-Identitaet:
//   ID:       net_sen_dht22_ref_01
//   Name:     NET-SEN DHT22 Reference
//   Variante: net_sen_dht22_reference
//   Caps:     TEMP | HUM
//
// DHT22-Parameter:
//   Warmup:          2500ms (Sensor braucht Zeit nach Power-On)
//   Read-Intervall:  2500ms
//   Temp-Bereich:    -40.0 bis +80.0 Grad C (in Zehntel: -400..800)
//   Feuchte-Bereich: 1% bis 100% (in Zehntel: 10..1000)
//   Hysterese:       Temp 10 (1.0 Grad), Feuchte 50 (5.0%)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_SEN_DEVICE_ID "net_sen_dht22_ref_01"
#define NET_SEN_DEVICE_NAME "NET-SEN DHT22 Reference"
#define NET_SEN_FW_VARIANT "net_sen_dht22_reference"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM)

#define NET_SEN_ENABLE_I2C_BASE 0  // Kein I2C

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_SEN_HEARTBEAT_INTERVAL_MS 60000UL
#define NET_SEN_STATE_INTERVAL_MS 60000UL

// DHT22-Timing
#define NET_SEN_DHT22_REF_WARMUP_MS 2500UL
#define NET_SEN_DHT22_REF_READ_INTERVAL_MS 2500UL
#define NET_SEN_DHT22_REF_ERROR_LOG_INTERVAL_MS 15000UL
#define NET_SEN_DHT22_REF_VALUE_LOG_INTERVAL_MS 15000UL

// DHT22-Plausibilitaetsgrenzen
#define NET_SEN_DHT22_REF_TEMP_MIN_01C (-400)   // -40.0 Grad C
#define NET_SEN_DHT22_REF_TEMP_MAX_01C 800      // +80.0 Grad C
#define NET_SEN_DHT22_REF_HUM_MIN_01PCT 10U     // 1.0%
#define NET_SEN_DHT22_REF_HUM_MAX_01PCT 1000U   // 100.0%

// Hysterese – Aenderung muss diese Schwellen ueberschreiten
#define NET_SEN_DHT22_REF_TEMP_DELTA_01C 10     // 1.0 Grad
#define NET_SEN_DHT22_REF_HUM_DELTA_01PCT 50U   // 5.0%
