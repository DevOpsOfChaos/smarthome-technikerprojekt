/**
 * @file main.cpp
 * @brief NET-ERL Kitchen Light: Reines Relais ohne Sensorik (Thin-Wrapper)
 *
 * @details Thin-Wrapper fuer NetErlRuntime. Kein Bewegungssensor, kein Umweltsensor.
 *          Relais per Master-CMD (MQTT->ESP-NOW) oder Button schaltbar.
 *          Auto-Light-Logik deaktiviert (keine Sensordaten).
 *
 * Hardware:   ESP32-C3 + 1 Relais
 * Pattern:    Thin-Wrapper – Hooks in NetErlRuntime.h eingehängt
 *
 * @author DevOpsOfChaos
 * @date   2026-05-15
 */

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

/**
 * @brief Device-Init-Hook – keine Sensoren zu initialisieren.
 * Wird von NetErlRuntime beim Boot aufgerufen.
 */
void netErlDeviceInit() {
    // Keine Sensoren zu initialisieren – nur Relais-Pin (von Runtime gesetzt)
}

/** @brief Keine Sensor-Defaults zurückzusetzen. */
void netErlDeviceResetSensorDefaults() {
    // Keine Sensorwerte zurückzusetzen
}

/**
 * @brief Bewegungserkennung – immer false (kein PIR-Sensor).
 * @return false
 */
bool netErlDeviceReadPresence() {
    // Kein Bewegungssensor → immer false
    return false;
}

/**
 * @brief Setzt den Relais-Ausgang (PIN_RELAY_1).
 * @param on true = Relais aktiv (HIGH je nach RELAY_1_ACTIVE_HIGH)
 */
void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
}

/**
 * @brief Sensor-Poll – keine Sensoren vorhanden, setzt fault=false.
 * @param now aktueller millis()-Wert (ungenutzt)
 */
void netErlDevicePollSensors(unsigned long) {
    // Keine Sensoren zu pollen
    runtime.fault = false;
}

/**
 * @brief Befuellt STATE-Payload: nur Relais-Status, alle Sensorwerte auf UNGUELTIG.
 *
 * @param[out] payload Zeiger auf RelayComfortConfigStateReportPayload
 * @param[out] size    Geschriebene Payload-Groesse in Bytes
 * @note temp_01c=INT16_MIN, hum_01pct/lux=0xFFFF, motion/auto_flags=0
 */
void netErlDeviceFillStatePayload(void* payload, size_t* size) {
    // Minimal-State: nur Relais-Status, keine Sensorwerte
    SmartHome::RelayComfortConfigStateReportPayload* p =
        static_cast<SmartHome::RelayComfortConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        safeStrCopy(p->node_id, sizeof(p->node_id), DEVICE_ID);
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

/** @brief Keine Auto-Light-Logik → immer 0. @return 0 */
uint8_t netErlDeviceBuildAutoFlags() {
    // Keine Auto-Light-Logik → keine Flags
    return 0;
}

/** @brief Keine Sensoren → kein Fehler. @return false */
bool netErlDeviceHasSensorFault() {
    return false;  // Keine Sensoren → kein Fehler
}

/** @brief Loggt aktuellen Relais-Status (1/0). */
void netErlDeviceLogSnapshot() {
    logMsg("INFO", "snap r=%s", runtime.relay_1 ? "1" : "0");
}
