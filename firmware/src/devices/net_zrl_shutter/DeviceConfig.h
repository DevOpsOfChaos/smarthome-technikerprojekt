// =============================================================================
// DeviceConfig.h – NET-ZRL Shutter: Geraetespezifische Konfiguration
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_zrl_shutter/DeviceConfig.h
// Hardware:   ESP32-C3 + 2-Relais-Rollo (Auf/Ab)
//
// Ueberschreibt die Defaults aus dem net_zrl-Basistyp.
// Alle nicht aufgeführten Werte erben vom Basistyp.
// =============================================================================

#pragma once

// -- Geraete-Identifikation --
#define NET_ZRL_DEVICE_ID           "NET-ZRL-002"
#define NET_ZRL_DEVICE_NAME         "NET-ZRL Shutter"
#define NET_ZRL_FW_VARIANT          "net_zrl_shutter"

// -- Debug --
#define NET_ZRL_DEBUG_ENABLED       1

// -- Relais-Pin-Mapping --
#define NET_ZRL_RELAY_UP_PIN        10
#define NET_ZRL_RELAY_DOWN_PIN      5
#define NET_ZRL_RELAY_UP_ACTIVE_HIGH    1
#define NET_ZRL_RELAY_DOWN_ACTIVE_HIGH  1

// -- Taster-Pins --
#define NET_ZRL_BUTTON_UP_PIN       2
#define NET_ZRL_BUTTON_DOWN_PIN     4
#define NET_ZRL_BUTTON_STOP_PIN     3

// -- LED-Pins --
#define NET_ZRL_LED_UP_PIN          6
#define NET_ZRL_LED_DOWN_PIN        7
#define NET_ZRL_LED_ACTIVE_HIGH     1

// -- Pegel-Logik --
#define NET_ZRL_BUTTON_ACTIVE_LOW   0
