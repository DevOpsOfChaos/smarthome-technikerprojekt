#pragma once

#include "../../../include/HardwarePinStandard.h"

// Device-spezifischer ADC-Pin fuer den Regenpfad.
#define BAT_SEN_RAIN_SIGNAL_PIN 3

#define BAT_SEN_PIN_STATUS_LED -1
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_RAIN_SIGNAL_PIN

#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 0
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
