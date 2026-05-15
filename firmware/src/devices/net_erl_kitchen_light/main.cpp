// =============================================================================
// main.cpp – NET-ERL Kitchen Light: Reines Relais (THIN)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_kitchen_light/main.cpp
// Hardware:   ESP32-C3 + 1 Relais (kein Sensor)
// Pattern:    Thin-Wrapper – Hooks in NetErlRuntime.h eingehängt
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Besonderheiten:
//   - Kein Bewegungssensor → Auto-Light deaktiviert
//   - Kein Umweltsensor → STATE ohne Temperatur/Feuchte/Lux
//   - Relais per Master-CMD oder Button schaltbar
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

// -- Baukasten-Defines (vor NetErlRuntime.h) --
#define NET_ERL_STORAGE_NS              "net_erl_kl"
#define NET_ERL_SENSOR_MASK             "XXXXXXX"
#define NET_ERL_INPUT_MASK              "BXXXX"
#define NET_ERL_PERSISTED_MAGIC         0x4B4C5431UL
#define NET_ERL_PERSISTED_KEY           "kitlight_cfg"
#define NET_ERL_DEVICE_PAGE_TITLE       "NET-ERL Kitchen Light"
#define NET_ERL_DEVICE_SECTION_TITLE    "Kitchen Light"
#define NET_ERL_DEVICE_SECTION_INTRO    "Keine Sensorik – nur Relais-Steuerung."

// Keine Auto-Light-Logik → Lux-Schwellen deaktivieren
// (Die Runtime handled das, aber ohne Sensor sind die Werte immer UNGUELTIG)

// -- Hooks aktivieren --
#define NET_ERL_DEVICE_HAS_CUSTOM_HOOKS 1

// =============================================================================
// RUNTIME (Baukasten Block 3 – liefert setup() und loop())
// =============================================================================
#include "../../basetypes/net_erl/NetErlRuntime.h"

// =============================================================================
// CUSTOM HOOKS (von NetErlRuntime.h aufgerufen)
// =============================================================================

void netErlDeviceInit() {
    // Keine Sensoren zu initialisieren – nur Relais-Pin (von Runtime gesetzt)
}

void netErlDeviceResetSensorDefaults() {
    // Keine Sensorwerte zurückzusetzen
}

bool netErlDeviceReadPresence() {
    // Kein Bewegungssensor → immer false
    return false;
}

void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
}

void netErlDevicePollSensors(unsigned long) {
    // Keine Sensoren zu pollen
    runtime.fault = false;
}

void netErlDeviceFillStatePayload(void* payload, size_t* size) {
    // Minimal-State: nur Relais-Status, keine Sensorwerte
    SmartHome::RelayComfortConfigStateReportPayload* p =
        static_cast<SmartHome::RelayComfortConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        cpy(p->node_id, sizeof(p->node_id), DEVICE_ID);
        p->relay_1 = runtime.relay_1 ? 1U : 0U;
        p->temp_01c = INT16_MIN;
        p->hum_01pct = 0xFFFFU;
        p->lux = 0xFFFFU;
        p->motion = 0U;
        p->auto_flags = 0U;  // Keine Auto-Light-Logik aktiv
        p->fault = runtime.fault ? 1U : 0U;
        p->report_interval_s = (uint16_t)runtime.report_interval_s;
        p->auto_on_lux_threshold = 0U;
    }
    if (size != nullptr) *size = sizeof(SmartHome::RelayComfortConfigStateReportPayload);
}

uint8_t netErlDeviceBuildAutoFlags() {
    // Keine Auto-Light-Logik → keine Flags
    return 0;
}

bool netErlDeviceHasSensorFault() {
    return false;  // Keine Sensoren → kein Fehler
}

void netErlDeviceLogSnapshot() {
    logMsg("INFO", "snap r=%s", runtime.relay_1 ? "1" : "0");
}
