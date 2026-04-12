#pragma once

#include "../../../include/HardwarePinStandard.h"

#ifndef BAT_SEN_PIN_STATUS_LED
#define BAT_SEN_PIN_STATUS_LED -1
#endif

#ifndef BAT_SEN_PIN_BATTERY_ADC
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#endif

#ifndef BAT_SEN_PIN_WAKE_INPUT
#define BAT_SEN_PIN_WAKE_INPUT SmartHome::HardwarePinStandard::PIN_BOOT_BUTTON
#endif

#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN 2
#endif

#ifndef SETUP_BUTTON_ACTIVE_LOW
#define SETUP_BUTTON_ACTIVE_LOW 0
#endif

#ifndef SETUP_BUTTON_HOLD_MS
#define SETUP_BUTTON_HOLD_MS 5000UL
#endif

#ifndef SETUP_INDICATOR_LED_PIN
#define SETUP_INDICATOR_LED_PIN 7
#endif

#ifndef SETUP_INDICATOR_LED_ACTIVE_HIGH
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#endif

#ifndef SETUP_INDICATOR_BLINK_MS
#define SETUP_INDICATOR_BLINK_MS 500UL
#endif

constexpr int PIN_STATUS_LED = BAT_SEN_PIN_STATUS_LED;
constexpr int PIN_BATTERY_ADC = BAT_SEN_PIN_BATTERY_ADC;
constexpr int PIN_WAKE_INPUT = BAT_SEN_PIN_WAKE_INPUT;
