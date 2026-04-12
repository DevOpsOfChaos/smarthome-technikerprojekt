#pragma once

#include "../../../include/HardwarePinStandard.h"

// Device-spezifischer ADC-Pin fuer den Regenpfad.
#define BAT_SEN_RAIN_SIGNAL_PIN 3

#define BAT_SEN_PIN_STATUS_LED -1
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_RAIN_SIGNAL_PIN

// Keine belastbar belegte Setup-Hardware auf diesem konkreten Geraet.
#define SETUP_BUTTON_PIN -1
#define SETUP_INDICATOR_LED_PIN -1
