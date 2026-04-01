#pragma once

// ============================================================
// Geräteklassen (device_class)
// ============================================================
#define SH_CLASS_NET_ERL    0x01U
#define SH_CLASS_NET_ZRL    0x02U
#define SH_CLASS_NET_SEN    0x03U
#define SH_CLASS_BAT_SEN    0x04U
#define SH_CLASS_MASTER     0xFEU
#define SH_CLASS_UNKNOWN    0xFFU

// ============================================================
// Power-Typen (power_type)
// ============================================================
#define SH_POWER_MAINS      0x00U
#define SH_POWER_BATTERY    0x01U

// ============================================================
// Meta-Schema-Version
// ============================================================
#define SH_META_SCHEMA_VERSION_1        0x01U
#define SH_META_SCHEMA_VERSION_CURRENT  SH_META_SCHEMA_VERSION_1

// ============================================================
// Control-Modes (control_mode)
// ============================================================
#define SH_CONTROL_MODE_NONE             0x00U
#define SH_CONTROL_MODE_RELAY            0x01U
#define SH_CONTROL_MODE_RELAY_LIGHT      0x02U
#define SH_CONTROL_MODE_DUAL_RELAY       0x03U
#define SH_CONTROL_MODE_DUAL_RELAY_LIGHT 0x04U
#define SH_CONTROL_MODE_COVER            0x05U

// ============================================================
// Config-Profile (config_profile)
// ============================================================
#define SH_PROFILE_NONE           0x00U
#define SH_PROFILE_HALL_LIGHT     0x01U
#define SH_PROFILE_KITCHEN_LIGHT  0x02U
#define SH_PROFILE_COVER_BASIC    0x03U

// ============================================================
// Reporting-Modes (reporting_mode)
// ============================================================
#define SH_REPORTING_PERIODIC        0x01U
#define SH_REPORTING_EVENT_DRIVEN    0x02U
#define SH_REPORTING_HYBRID          0x03U
#define SH_REPORTING_SLEEP_PERIODIC  0x04U
#define SH_REPORTING_SLEEP_EVENT     0x05U

// ============================================================
// Fähigkeits-Bitmasks
// ============================================================
#define SH_CAP_RELAY        0x0001U
#define SH_CAP_RELAY2       0x0002U
#define SH_CAP_TEMP         0x0004U
#define SH_CAP_HUM          0x0008U
#define SH_CAP_LUX          0x0010U
#define SH_CAP_AQI          0x0020U
#define SH_CAP_MOTION       0x0040U
#define SH_CAP_WINDOW       0x0080U
#define SH_CAP_RAIN         0x0100U
#define SH_CAP_BATTERY      0x0200U
#define SH_CAP_BUTTON       0x0400U
#define SH_CAP_MULTIBUTTON  0x0800U
#define SH_CAP_LED_RING     0x1000U
#define SH_CAP_COVER        0x2000U
#define SH_CAP_SETUP_PORTAL 0x4000U
#define SH_CAP_PRESSURE     0x8000U
