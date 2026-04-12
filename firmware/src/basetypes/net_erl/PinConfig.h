#pragma once

#include "../../../include/HardwarePinStandard.h"

// Neutraler NET-ERL-Basistyp: nur Relais und optionale Status-LED.

#ifndef NET_ERL_PIN_RELAY_1
#define NET_ERL_PIN_RELAY_1 SmartHome::HardwarePinStandard::PIN_RELAY_1
#endif

#ifndef NET_ERL_PIN_STATUS_LED
#define NET_ERL_PIN_STATUS_LED -1
#endif

#ifndef NET_ERL_RELAY_1_ACTIVE_HIGH
#define NET_ERL_RELAY_1_ACTIVE_HIGH 1
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

#define PIN_RELAY_1 NET_ERL_PIN_RELAY_1
#define PIN_STATUS_LED NET_ERL_PIN_STATUS_LED
#define RELAY_1_ACTIVE_HIGH NET_ERL_RELAY_1_ACTIVE_HIGH
