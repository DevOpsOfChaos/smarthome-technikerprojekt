#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Konkretes Geraet: netzbetriebener Raum-Sensor.
// Diese Datei setzt nur feste Defaultwerte fuer den Basistyp net_sen.

#define NET_SEN_DEVICE_ID "net_sen_01"
#define NET_SEN_DEVICE_NAME "NET-SEN Room"
#define NET_SEN_FW_VARIANT "net_sen_room"
#define NET_SEN_DEVICE_CAPS (SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX)

#define NET_SEN_ENABLE_I2C_BASE 1

#define NET_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_SEN_HEARTBEAT_INTERVAL_MS 60000UL
#define NET_SEN_STATE_INTERVAL_MS 60000UL

#define NET_SEN_ROOM_SENSOR_READ_INTERVAL_MS 2500UL
#define NET_SEN_ROOM_TEMP_DELTA_01C 10
#define NET_SEN_ROOM_HUM_DELTA_01PCT 50U
#define NET_SEN_ROOM_LUX_DELTA 25U

#define NET_SEN_ROOM_USE_BME280 1
#define NET_SEN_ROOM_BME280_ADDRESS 0x76

#define NET_SEN_ROOM_USE_VEML7700 1
