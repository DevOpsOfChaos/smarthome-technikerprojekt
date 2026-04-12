#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Konkretes Geraet: einfacher Regensensor auf neutralem BAT-SEN-Basistyp.
#define BAT_SEN_DEVICE_ID "bat_sen_02"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Rain"
#define BAT_SEN_FW_VARIANT "bat_sen_rain"
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_RAIN)

#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// V1-Defaults: schlank, Batterieprofil wird bewusst hier gewaehlt.
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_2X_AA
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U

// V1 bleibt timerbasiert, GPIO-Wake ist bewusst nicht aktiv.
#define BAT_SEN_ENABLE_GPIO_WAKE 0

// Device-spezifische Regenparameter.
#define BAT_SEN_RAIN_SAMPLE_INTERVAL_MS 200UL
#define BAT_SEN_RAIN_STATE_DELTA_RAW 25U
#define BAT_SEN_RAIN_WET_THRESHOLD_RAW 2200U
#define BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW 2050U
#define BAT_SEN_RAIN_LEVEL_HIGH_IS_WET 1
