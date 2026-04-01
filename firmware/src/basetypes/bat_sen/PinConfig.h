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

constexpr int PIN_STATUS_LED = BAT_SEN_PIN_STATUS_LED;
constexpr int PIN_BATTERY_ADC = BAT_SEN_PIN_BATTERY_ADC;
constexpr int PIN_WAKE_INPUT = BAT_SEN_PIN_WAKE_INPUT;
