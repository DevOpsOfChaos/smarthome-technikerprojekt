/**
 * @file main.cpp
 * @brief NET-ZRL Shutter: Rollo-Steuerung (Thin-Wrapper)
 *
 * @details Ultra-Thin-Wrapper – alle Logik liegt im net_zrl-Basistyp
 *          (../../basetypes/net_zrl/main.cpp). Dieser Device-Adapter
 *          bindet nur DeviceConfig.h ein und inkludiert den Basistyp.
 *
 * Hardware:   ESP32-C3 + 2 Relais (Auf/Ab) + 3 Taster + 2 LEDs
 * Pattern:    Thin-Wrapper – alle Logik im net_zrl-Basistyp
 *
 * @author DevOpsOfChaos
 * @date   2026-05-15
 */

#include <Arduino.h>

#include "DeviceConfig.h"

// -- Basistyp einbinden (liefert setup() und loop()) --
#include "../../basetypes/net_zrl/main.cpp"
