// =============================================================================
// main.cpp – NET-ZRL Shutter: Rollo-Steuerung (THIN)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_zrl_shutter/main.cpp
// Hardware:   ESP32-C3 + 2 Relais (Auf/Ab) + 3 Taster + 2 LEDs
// Pattern:    Thin-Wrapper – alle Logik im net_zrl-Basistyp
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#include <Arduino.h>

#include "DeviceConfig.h"

// -- Basistyp einbinden (liefert setup() und loop()) --
#include "../../basetypes/net_zrl/main.cpp"
