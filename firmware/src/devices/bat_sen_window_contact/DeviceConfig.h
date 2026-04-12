#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// Konkretes Geraet: einfacher Fensterkontakt auf neutralem BAT-SEN-Basistyp.
#define BAT_SEN_DEVICE_ID "bat_sen_01"
#define BAT_SEN_DEVICE_NAME "BAT-SEN Window"
#define BAT_SEN_FW_VARIANT "bat_sen_window_contact"
#define BAT_SEN_DEVICE_CAPS (SH_CAP_BATTERY | SH_CAP_WINDOW)

#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT

// V1-Defaults: klar, klein, Batterieprofil wird bewusst hier gewaehlt.
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_CR2032
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 5000U

// C3-Device nutzt GPIO-Wake fuer echten Deep-Sleep-Wakeup am Kontaktpin.
#define BAT_SEN_ENABLE_GPIO_WAKE 1

// Wake-Bedingung folgt dem als "open" gewerteten Pegel.
#define BAT_SEN_GPIO_WAKE_LEVEL_HIGH BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH

// Device-spezifische Kontaktparameter.
#define BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS 35UL
#define BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP 1
#define BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH 1
