#pragma once

#include "../../../include/HardwarePinStandard.h"

#ifndef MASTER_PIN_STATUS_LED
#define MASTER_PIN_STATUS_LED -1
#endif

#ifndef MASTER_PIN_BUTTON_1
#define MASTER_PIN_BUTTON_1 -1
#endif

#ifndef MASTER_PIN_SENSOR_SDA
#define MASTER_PIN_SENSOR_SDA SmartHome::HardwarePinStandard::PIN_I2C_SDA
#endif

#ifndef MASTER_PIN_SENSOR_SCL
#define MASTER_PIN_SENSOR_SCL SmartHome::HardwarePinStandard::PIN_I2C_SCL
#endif

#ifndef MASTER_PIN_INTERNAL_NEOPIXEL
#define MASTER_PIN_INTERNAL_NEOPIXEL SmartHome::HardwarePinStandard::GPIO_INTERNAL_NEOPIXEL
#endif

// Der Master nutzt aktuell keine Relais und keine Pflichtsensorik.
// Die Datei legt nur die feste Board-nahe Grundbelegung ab.

constexpr int PIN_STATUS_LED = MASTER_PIN_STATUS_LED;
constexpr int PIN_BUTTON_1 = MASTER_PIN_BUTTON_1;
constexpr int PIN_SENSOR_SDA = MASTER_PIN_SENSOR_SDA;
constexpr int PIN_SENSOR_SCL = MASTER_PIN_SENSOR_SCL;
constexpr int PIN_INTERNAL_NEOPIXEL = MASTER_PIN_INTERNAL_NEOPIXEL;
