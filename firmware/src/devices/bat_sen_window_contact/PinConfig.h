#pragma once

#include "../../../include/HardwarePinStandard.h"

// Erstes v1-Device nutzt den standardisierten Boot-Button-Pin als Kontaktpfad.
#define BAT_SEN_WINDOW_CONTACT_PIN SmartHome::HardwarePinStandard::PIN_BOOT_BUTTON

#define BAT_SEN_PIN_STATUS_LED -1
#define BAT_SEN_PIN_BATTERY_ADC SmartHome::HardwarePinStandard::PIN_BATTERY_ADC
#define BAT_SEN_PIN_WAKE_INPUT BAT_SEN_WINDOW_CONTACT_PIN
