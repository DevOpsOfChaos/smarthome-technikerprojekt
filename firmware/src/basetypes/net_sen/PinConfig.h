#pragma once

#include "../../../include/HardwarePinStandard.h"

#ifndef NET_SEN_PIN_STATUS_LED
#define NET_SEN_PIN_STATUS_LED -1
#endif

#ifndef NET_SEN_PIN_SENSOR_SDA
#define NET_SEN_PIN_SENSOR_SDA SmartHome::HardwarePinStandard::PIN_I2C_SDA
#endif

#ifndef NET_SEN_PIN_SENSOR_SCL
#define NET_SEN_PIN_SENSOR_SCL SmartHome::HardwarePinStandard::PIN_I2C_SCL
#endif

#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN -1
#endif

#ifndef SETUP_BUTTON_ACTIVE_LOW
#define SETUP_BUTTON_ACTIVE_LOW 1
#endif

#ifndef SETUP_BUTTON_HOLD_MS
#define SETUP_BUTTON_HOLD_MS 5000UL
#endif

#ifndef SETUP_INDICATOR_LED_PIN
#define SETUP_INDICATOR_LED_PIN -1
#endif

#ifndef SETUP_INDICATOR_LED_ACTIVE_HIGH
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#endif

#ifndef SETUP_INDICATOR_BLINK_MS
#define SETUP_INDICATOR_BLINK_MS 500UL
#endif

constexpr int PIN_STATUS_LED = NET_SEN_PIN_STATUS_LED;
constexpr int PIN_SENSOR_SDA = NET_SEN_PIN_SENSOR_SDA;
constexpr int PIN_SENSOR_SCL = NET_SEN_PIN_SENSOR_SCL;
