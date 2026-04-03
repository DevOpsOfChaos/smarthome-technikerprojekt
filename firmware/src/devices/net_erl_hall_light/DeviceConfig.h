#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Enger Debugpfad fuer die net_erl-Hall-Light-Projektion.
// Sensorik aktiv, PIR aktiv, aber PIR schaltet in diesem Lauf nicht das Relais.
// Ziel: Payload / Master-Projektion / GPIO10 sauber trennen.

#define NET_ERL_DEVICE_ID "net_erl_01"
#define NET_ERL_DEVICE_NAME "NET-ERL Hall Light Projection Debug"
#define NET_ERL_FW_VARIANT "net_erl_hall_light_projection_debug"

#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY | SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_MOTION)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_HALL_LIGHT
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

#define NET_ERL_SENSOR_POLL_INTERVAL_MS 250UL
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS 2000UL
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 15000UL

// Wichtige Debug-Trennung:
#define NET_ERL_DEBUG_IGNORE_PIR_FOR_RELAY 1
