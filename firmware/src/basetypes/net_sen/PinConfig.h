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

constexpr int PIN_STATUS_LED = NET_SEN_PIN_STATUS_LED;
constexpr int PIN_SENSOR_SDA = NET_SEN_PIN_SENSOR_SDA;
constexpr int PIN_SENSOR_SCL = NET_SEN_PIN_SENSOR_SCL;
