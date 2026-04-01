#pragma once

#include "../../../include/HardwarePinStandard.h"

// Device-spezifischer Kontakt-Pin fuer ESP32-C3-Deep-Sleep-Wakeup:
// GPIO3 ist wake-faehig (C3: gueltig sind GPIO0..GPIO5) und trennt den
// Fensterkontakt sauber vom Boot-Button-Standardpin GPIO9.
#define BAT_SEN_WINDOW_CONTACT_PIN 3

#define BAT_SEN_PIN_STATUS_LED -1
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_WINDOW_CONTACT_PIN
