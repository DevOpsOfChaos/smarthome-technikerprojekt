// =============================================================================
// DeviceConfig.h – NET-ERL Kitchen Light: Reines Relais (kein Sensor)
// =============================================================================
#pragma once

// -- Geraete-Identifikation --
#define NET_ERL_DEVICE_ID               "NET-ERL-003"
#define NET_ERL_DEVICE_NAME             "NET-ERL Kitchen Light"
#define NET_ERL_FW_VARIANT              "net_erl_kitchen_light"
#define NET_ERL_DEVICE_CAPS             (SH_CAP_RELAY | SH_CAP_BUTTON)
#define NET_ERL_DEVICE_CONTROL_MODE     SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE   SH_PROFILE_KITCHEN_LIGHT
#define NET_ERL_DEVICE_REPORTING_MODE   SH_REPORTING_HYBRID

// -- Debug --
#define NET_ERL_DEBUG_ENABLED           0

// -- Timing --
#define NET_ERL_WLAN_CHANNEL            6
#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL
#define NET_ERL_HEARTBEAT_INTERVAL_MS   20000UL
#define NET_ERL_LOOP_INTERVAL_MS        20UL
#define NET_ERL_MIN_REPORT_INTERVAL_S   5U
#define NET_ERL_MAX_REPORT_INTERVAL_S   600U
#define NET_ERL_BOOT_COUNTER            1U
#define NET_ERL_DEFAULT_REPORT_INTERVAL_S 10U
#define NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD 250U
#define NET_ERL_DEFAULT_AUTO_OFF_DELAY_S 15U
#define NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS 30000UL
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 30000UL

// -- Sensor-Poll (kein Sensor, aber Runtime braucht den Wert) --
#define NET_ERL_SENSOR_POLL_INTERVAL_MS 1000UL
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS  1000UL

// -- Relais-Pin --
#define PIN_RELAY_1             10
#define RELAY_1_ACTIVE_HIGH     1
