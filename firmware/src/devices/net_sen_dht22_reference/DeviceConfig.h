#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Konkretes Geraet: einfacher net_sen-Referenzpfad mit DHT22.
// Ziel ist ein frueher, real pruefbarer Temperatur-/Feuchtepfad ohne Zusatzsensorik.

#define NET_SEN_DEVICE_ID "net_sen_dht22_ref_01"
#define NET_SEN_DEVICE_NAME "NET-SEN DHT22 Reference"
#define NET_SEN_FW_VARIANT "net_sen_dht22_reference"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM)

#define NET_SEN_ENABLE_I2C_BASE 0

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_SEN_HEARTBEAT_INTERVAL_MS 60000UL
#define NET_SEN_STATE_INTERVAL_MS 60000UL

#define NET_SEN_DHT22_REF_WARMUP_MS 2500UL
#define NET_SEN_DHT22_REF_READ_INTERVAL_MS 2500UL
#define NET_SEN_DHT22_REF_ERROR_LOG_INTERVAL_MS 15000UL
#define NET_SEN_DHT22_REF_VALUE_LOG_INTERVAL_MS 15000UL
#define NET_SEN_DHT22_REF_TEMP_MIN_01C (-400)
#define NET_SEN_DHT22_REF_TEMP_MAX_01C 800
#define NET_SEN_DHT22_REF_HUM_MIN_01PCT 10U
#define NET_SEN_DHT22_REF_HUM_MAX_01PCT 1000U

#define NET_SEN_DHT22_REF_TEMP_DELTA_01C 10
#define NET_SEN_DHT22_REF_HUM_DELTA_01PCT 50U
