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

#define PIN_RELAY_1 NET_ERL_PIN_RELAY_1
#define PIN_STATUS_LED NET_ERL_PIN_STATUS_LED
#define RELAY_1_ACTIVE_HIGH NET_ERL_RELAY_1_ACTIVE_HIGH
