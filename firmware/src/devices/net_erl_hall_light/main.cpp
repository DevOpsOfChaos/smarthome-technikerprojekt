// =============================================================================
// main.cpp – NET-ERL Hall Light: Flurlicht mit PIR + Lux (THIN)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_hall_light/main.cpp
// Hardware:   ESP32-C3 + BME280 + VEML7700 + PIR + Relais
// Pattern:    Thin-Wrapper – Hooks in NetErlRuntime.h eingehängt
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-15 (Thin-Pattern-Migration)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

// -- Baukasten-Defines (vor NetErlRuntime.h) --
#define NET_ERL_STORAGE_NS              "net_erl_hl"
#define NET_ERL_SENSOR_MASK             "THLMXXXXXX"
#define NET_ERL_INPUT_MASK              "XXXXX"
#define NET_ERL_PERSISTED_MAGIC         0x484C4C31UL
#define NET_ERL_PERSISTED_KEY           "hall_setup_v1"
#define NET_ERL_DEVICE_PAGE_TITLE       "NET-ERL Hall Light"
#define NET_ERL_DEVICE_SECTION_TITLE    "Hall Light"
#define NET_ERL_DEVICE_SECTION_INTRO    "Lux-Schwelle und Nachlauf."

// Hall-spezifische Unterschiede zum Kitchen
#define NET_ERL_USE_ISR_CMD_QUEUE       1   // ISR-safe CMD-Queue
#define NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION 0  // Nachlauf NICHT verlängern
#define NET_ERL_WDT_TIMEOUT_S           8UL

// -- Hooks aktivieren --
#define NET_ERL_DEVICE_HAS_CUSTOM_HOOKS 1

// =============================================================================
// DEVICE-SPEZIFISCHE OBJEKTE
// =============================================================================

Adafruit_BME280 bme280;
Adafruit_VEML7700 veml7700 = Adafruit_VEML7700();

// =============================================================================
// DEVICE-SPEZIFISCHER ZUSTAND
// =============================================================================
namespace {
    bool bme280_ok = false, veml7700_ok = false;
    unsigned long letzter_bme_recovery_ms = 0, letzter_veml_recovery_ms = 0;
    unsigned long letztes_env_sample_ms = 0;
    unsigned long veml7700_bereit_seit_ms = 0;

    // Sensor-Messwerte
    int16_t temp_01c = INT16_MIN;
    uint16_t hum_01pct = 0xFFFFU, lux = 0xFFFFU;
    bool pir_raw = false;

    constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 250UL;
}

// =============================================================================
// SENSOR-HILFSFUNKTIONEN (device-spezifisch)
// =============================================================================
namespace {
    bool initBme280() {
        const uint8_t addrs[] = {(uint8_t)NET_ERL_BME280_ADDRESS};
        for (uint8_t a : addrs) {
            if (!bme280.begin(a, &Wire)) continue;
            return true;
        }
        return false;
    }

    void konfVeml7700() {
        veml7700.setGain(VEML7700_GAIN_1);
        veml7700.setIntegrationTime(VEML7700_IT_100MS);
    }

    bool initVeml7700(unsigned long jetzt) {
        if (!veml7700.begin()) return false;
        konfVeml7700();
        veml7700_bereit_seit_ms = jetzt;
        return true;
    }
}

// =============================================================================
// CUSTOM HOOKS (von NetErlRuntime.h aufgerufen)
// =============================================================================

void netErlDeviceInit() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);

    bme280_ok = initBme280();
    if (!bme280_ok) logf("WARN", "BME280 init fail");

    veml7700_ok = initVeml7700(millis());
    if (!veml7700_ok) logf("WARN", "VEML7700 init fail");

    pinMode(PIN_PIR, INPUT);
}

void netErlDeviceResetSensorDefaults() {
    temp_01c = INT16_MIN; hum_01pct = 0xFFFFU; lux = 0xFFFFU;
}

bool netErlDeviceReadPresence() {
    pir_raw = (digitalRead(PIN_PIR) == HIGH);
    return pir_raw;
}

void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
#if PIN_STATUS_LED >= 0
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
#endif
}

void netErlDevicePollSensors(unsigned long nowMs) {
    if ((nowMs - letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    letztes_env_sample_ms = nowMs;

    // Recovery: ausgefallene Sensoren periodisch neu initialisieren
    if (!bme280_ok && recoveryIsDue(letzter_bme_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_bme_recovery_ms = nowMs; bme280_ok = initBme280();
    }
    if (!veml7700_ok && recoveryIsDue(letzter_veml_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_veml_recovery_ms = nowMs; veml7700_ok = initVeml7700(nowMs);
    }

    // BME280 lesen
    if (bme280_ok) {
        float t = bme280.readTemperature();
        float h = bme280.readHumidity();
        if (!isnan(t) && !isnan(h) && h >= 0 && h <= 100) {
            temp_01c = (int16_t)lroundf(t * 10.0f);
            hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
        } else { bme280_ok = false; logf("WARN", "BME280 unplausibel"); }
    }

    // VEML7700 lesen
    if (veml7700_ok) {
        float l = veml7700.readLux();
        if (!isnan(l) && l >= 0) lux = clampToU16((long)lroundf(l));
        else { veml7700_ok = false; logf("WARN", "VEML7700 read fail"); }
    }

    // Status aktualisieren
    runtime.fault = !(bme280_ok && veml7700_ok);

    // Late-Lux: Auto-On-Entscheidung wenn Lux-Wert jetzt verfügbar ist
    if (runtime.motion_aktiv && runtime.pending_auto_on_decision && !runtime.relay_1 && lux != 0xFFFFU) {
        runtime.pending_auto_on_decision = false;
        if (lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true; runtime.blocked_by_lux = false;
            setRelay(true, "auto_on_late_lux");
            sendRelayEvent(SH_TRIGGER_AUTO);
            runtime.state_report_offen = true;
        } else {
            runtime.blocked_by_lux = true;
            runtime.state_report_offen = true;
        }
    }
}

void netErlDeviceFillStatePayload(void* payload, size_t* size) {
    SmartHome::RelayComfortConfigStateReportPayload* p =
        static_cast<SmartHome::RelayComfortConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        cpy(p->node_id, sizeof(p->node_id), DEVICE_ID);
        p->relay_1 = runtime.relay_1 ? 1U : 0U;
        p->temp_01c = temp_01c;
        p->hum_01pct = hum_01pct;
        p->lux = lux;
        p->motion = runtime.motion_aktiv ? 1U : 0U;
        p->auto_flags = netErlDeviceBuildAutoFlags();
        p->fault = runtime.fault ? 1U : 0U;
        p->report_interval_s = (uint16_t)runtime.report_interval_s;
        p->auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    }
    if (size != nullptr) *size = sizeof(SmartHome::RelayComfortConfigStateReportPayload);
}

uint8_t netErlDeviceBuildAutoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (veml7700_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    // Hall-spezifisch: BLOCKED_BY_MISSING_LUX Flag
    if (runtime.pending_auto_on_decision && lux == 0xFFFFU) f |= 0x10;
    return f;
}

bool netErlDeviceHasSensorFault() {
    return !(bme280_ok && veml7700_ok);
}

void netErlDeviceLogSnapshot() {
    logf("INFO", "snap t=%d h=%u l=%u m=%s r=%s auto=%s bl=%s fa=%s",
        (int)temp_01c, hum_01pct, lux,
        runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0",
        runtime.relay_auto_owned ? "1" : "0",
        runtime.blocked_by_lux ? "1" : "0",
        runtime.fault ? "1" : "0");
}

// =============================================================================
// RUNTIME (Baukasten Block 3 – liefert setup() und loop())
// =============================================================================
#include "../../basetypes/net_erl/NetErlRuntime.h"
