#pragma once

// Verbindlicher Hardware-Pinstandard fuer die aktuellen Basistyp-Platinen.
// Abweichungen muessen in der jeweiligen Geraetedoku sauber dokumentiert werden.

namespace SmartHome {
namespace HardwarePinStandard {

constexpr int PIN_I2C_SDA = 0;
constexpr int PIN_I2C_SCL = 1;
constexpr int GPIO_INTERNAL_NEOPIXEL = 8;
constexpr int PIN_BOOT_BUTTON = 9;
constexpr int PIN_RELAY_1 = 10;
constexpr int PIN_RELAY_2 = 5;
constexpr int PIN_BATTERY_ADC = 4;

}  // namespace HardwarePinStandard
}  // namespace SmartHome
