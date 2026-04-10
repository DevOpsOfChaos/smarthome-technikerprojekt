#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Belegter Minimalpfad aus dem real genutzten Blank-Relay-Stand.
// Dieser Basistyp bleibt bewusst schlank: 1 Relais, HELLO/ACK/STATE/CFG,
// keine Hallway- oder Komfortlogik.

#ifndef NET_ERL_DEVICE_ID
#define NET_ERL_DEVICE_ID "net_erl_01"
#endif

#ifndef NET_ERL_DEVICE_NAME
#define NET_ERL_DEVICE_NAME "NET-ERL Blank Relay Test"
#endif

#ifndef NET_ERL_FW_VARIANT
#define NET_ERL_FW_VARIANT "net_erl_base"
#endif

#ifndef NET_ERL_DEVICE_CAPS
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY)
#endif

#ifndef NET_ERL_DEVICE_CONTROL_MODE
#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY
#endif

#ifndef NET_ERL_DEVICE_CONFIG_PROFILE
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_NONE
#endif

#ifndef NET_ERL_DEVICE_REPORTING_MODE
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID
#endif

#ifndef NET_ERL_DEBUG_ENABLED
#define NET_ERL_DEBUG_ENABLED 1
#endif

#ifndef NET_ERL_WLAN_CHANNEL
#define NET_ERL_WLAN_CHANNEL 6
#endif

#ifndef NET_ERL_HELLO_RETRY_INTERVAL_MS
#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#endif

#ifndef NET_ERL_HEARTBEAT_INTERVAL_MS
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL
#endif

#ifndef NET_ERL_STATE_INTERVAL_MS
#define NET_ERL_STATE_INTERVAL_MS 10000UL
#endif

#ifndef NET_ERL_LOOP_INTERVAL_MS
#define NET_ERL_LOOP_INTERVAL_MS 20UL
#endif

#ifndef NET_ERL_MIN_REPORT_INTERVAL_S
#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#endif

#ifndef NET_ERL_MAX_REPORT_INTERVAL_S
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U
#endif

#ifndef NET_ERL_BOOT_COUNTER
#define NET_ERL_BOOT_COUNTER 1U
#endif
