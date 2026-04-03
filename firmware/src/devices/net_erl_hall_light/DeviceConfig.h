#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Blanker net_erl-Testpfad fuer den ersten echten Command-/ACK-Lauf.
// Noch keine Sensorik, nur Relaispfad und sauberer Protokollnachweis.

#define NET_ERL_DEVICE_ID "net_erl_01"
#define NET_ERL_DEVICE_NAME "NET-ERL Blank Relay Test"
#define NET_ERL_FW_VARIANT "net_erl_blank_test"

#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_NONE
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

#define NET_ERL_DEBUG_ENABLED 1
#define NET_ERL_WLAN_CHANNEL 6

#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL
#define NET_ERL_STATE_INTERVAL_MS 10000UL
#define NET_ERL_LOOP_INTERVAL_MS 20UL

#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U

#define NET_ERL_BOOT_COUNTER 1U
