// =============================================================================
// main.cpp – NET-ERL Kitchen: Kuechenlicht mit Radar + Luftqualitaet (THIN)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_kitchen/main.cpp
// Hardware:   ESP32-C3 + BME680 + VEML7700 + ENS160 + LD2410 + NeoPixel + Relais
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
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_NeoPixel.h>
#include <ScioSense_ENS160.h>

#ifndef ENS160_REG_TEMP_IN
#define ENS160_REG_TEMP_IN 0x13
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"

// -- Baukasten-Defines (vor NetErlRuntime.h) --
#define NET_ERL_STORAGE_NS              "net_erl_kit"
#define NET_ERL_SENSOR_MASK             "THLPGAMXXX"
#define NET_ERL_INPUT_MASK              "BXXXX"
#define NET_ERL_PERSISTED_MAGIC         0x4B544331UL
#define NET_ERL_PERSISTED_KEY           "kitchen_cfg_v1"
#define NET_ERL_DEVICE_PAGE_TITLE       "NET-ERL Kitchen"
#define NET_ERL_DEVICE_SECTION_TITLE    "Kitchen"
#define NET_ERL_DEVICE_SECTION_INTRO    "Lux-Schwelle und Nachlauf."
#define NET_ERL_HAS_BUTTON
#define NET_ERL_HAS_INDICATOR_UPDATE

// -- Hooks aktivieren --
#define NET_ERL_DEVICE_HAS_CUSTOM_HOOKS 1

// =============================================================================
// DEVICE-SPEZIFISCHE OBJEKTE
// =============================================================================

Adafruit_BME680 bme680;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
ScioSense_ENS160 ens160Addr52(NET_ERL_ENS160_PRIMARY_ADDRESS);
ScioSense_ENS160 ens160Addr53(NET_ERL_ENS160_FALLBACK_ADDRESS);
ScioSense_ENS160* ens160 = nullptr;
Adafruit_NeoPixel ledRing(LED_RING_COUNT, PIN_LED_RING, NEO_GRB + NEO_KHZ800);

// =============================================================================
// DEVICE-SPEZIFISCHER ZUSTAND
// =============================================================================
namespace {
    bool bme_ok = false, lux_ok = false, ens_ok = false;
    uint8_t bme680_adresse = 0, ens160_adresse = 0;
    uint8_t bme680_gueltige_messungen = 0;
    unsigned long letzter_bme_recovery_ms = 0, letzter_lux_recovery_ms = 0, letzter_ens_recovery_ms = 0;
    unsigned long letztes_env_sample_ms = 0, letzter_ens_gueltig_ms = 0;
    bool ring_initialized = false;

    // Sensor-Messwerte
    int16_t temp_01c = INT16_MIN;
    uint16_t hum_01pct = 0xFFFFU, lux = 0xFFFFU;
    uint32_t pressure_pa = 0xFFFFFFFFUL, gas_ohm = 0xFFFFFFFFUL;
    uint16_t aqi = 0xFFFFU, tvoc_ppb = 0xFFFFU, eco2_ppm = 0xFFFFU;
    bool ld2410_raw = false;

    constexpr uint32_t PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint32_t GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint16_t AIR_METRIC_UNGUELTIG = 0xFFFFU;
    constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;
    constexpr uint8_t LED_RING_HELLIGKEIT = 24U;
    constexpr uint16_t I2C_TIMEOUT_MS = 50U;
}

// =============================================================================
// SENSOR-HILFSFUNKTIONEN (device-spezifisch)
// =============================================================================
namespace {
    uint16_t mapAqi500(uint16_t r) { return (r >= 1 && r <= ENS160_AQI_MAX_BASIC) ? (uint16_t)(r * 100U) : 0U; }
    uint16_t encT(float c) { return (uint16_t)((c + 273.15f) * 64.0f); }
    uint16_t encH(float h) { return (uint16_t)(h * 512.0f); }

    int writeEnsEnv(uint8_t a, float t, float h) {
        uint8_t b[4]; uint16_t te = encT(t), he = encH(h);
        b[0] = te & 0xFF; b[1] = (te >> 8) & 0xFF; b[2] = he & 0xFF; b[3] = (he >> 8) & 0xFF;
        Wire.beginTransmission(a); Wire.write(ENS160_REG_TEMP_IN); Wire.write(b, 4); return Wire.endTransmission();
    }

    void konfVeml() { veml.setGain(VEML7700_GAIN_1); veml.setIntegrationTime(VEML7700_IT_400MS); }

    bool initBme() {
        uint8_t addrs[] = {(uint8_t)NET_ERL_BME680_PRIMARY_ADDRESS, (uint8_t)NET_ERL_BME680_FALLBACK_ADDRESS};
        for (uint8_t a : addrs) {
            if (!bme680.begin(a, &Wire)) continue;
            bme680.setTemperatureOversampling(BME680_OS_8X); bme680.setHumidityOversampling(BME680_OS_2X);
            bme680.setPressureOversampling(BME680_OS_4X); bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
            bme680.setGasHeater(320U, 150U); bme680_adresse = a; return true;
        }
        return false;
    }

    bool initEns() {
        ens160 = &ens160Addr52; if (ens160->begin()) { ens160_adresse = NET_ERL_ENS160_PRIMARY_ADDRESS; return true; }
        ens160 = &ens160Addr53; if (ens160->begin()) { ens160_adresse = NET_ERL_ENS160_FALLBACK_ADDRESS; return true; }
        ens160 = nullptr; return false;
    }

    bool gasWarmupOk(unsigned long j) {
        return gasWarmupComplete(runtime.boot_ms, j, NET_ERL_BME680_GAS_WARMUP_MS, bme680_gueltige_messungen, NET_ERL_BME680_GAS_WARMUP_MIN_READS);
    }

    bool ensStale(unsigned long j) {
        if (!ens_ok || !ens160) return true;
        return letzter_ens_gueltig_ms > 0 && (j - letzter_ens_gueltig_ms) > NET_ERL_ENS160_STALE_TIMEOUT_MS;
    }
}

// =============================================================================
// CUSTOM HOOKS (von NetErlRuntime.h aufgerufen)
// =============================================================================

void netErlDeviceInit() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
    Wire.setClock(NET_ERL_I2C_CLOCK_HZ);
    Wire.setTimeOut(I2C_TIMEOUT_MS);

    bme_ok = initBme();
    if (!bme_ok) logf("WARN", "BME680 init fail");

    if (!veml.begin()) { lux_ok = false; logf("WARN", "VEML7700 init fail"); }
    else { lux_ok = true; konfVeml(); }

    ens_ok = initEns();
    if (!ens_ok) { logf("WARN", "ENS160 init fail"); }
    else if (!ens160->setMode(ENS160_OPMODE_STD)) { ens_ok = false; logf("WARN", "ENS160 mode fail"); }

    pinMode(PIN_LD2410_OUT, INPUT);
}

void netErlDeviceResetSensorDefaults() {
    temp_01c = INT16_MIN; hum_01pct = 0xFFFFU; lux = 0xFFFFU;
    pressure_pa = PRESSURE_UNGUELTIG; gas_ohm = GAS_OHM_UNGUELTIG;
    aqi = AIR_METRIC_UNGUELTIG; tvoc_ppb = AIR_METRIC_UNGUELTIG; eco2_ppm = AIR_METRIC_UNGUELTIG;
}

bool netErlDeviceReadPresence() {
    ld2410_raw = (digitalRead(PIN_LD2410_OUT) == HIGH);
    return ld2410_raw;
}

void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
}

void netErlDeviceUpdateIndicators(bool relayOn) {
    if (!ring_initialized) {
        ledRing.begin(); ledRing.setBrightness(LED_RING_HELLIGKEIT);
        ledRing.clear(); ledRing.show(); ring_initialized = true;
    }
    if (relayOn) {
        uint32_t c = ledRing.Color(24, 24, 24);
        for (uint16_t i = 0; i < ledRing.numPixels(); ++i) ledRing.setPixelColor(i, c);
    } else {
        ledRing.clear();
    }
    ledRing.show();
}

void netErlDevicePollSensors(unsigned long nowMs) {
    if ((nowMs - letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    letztes_env_sample_ms = nowMs;

    // Recovery: ausgefallene Sensoren periodisch neu initialisieren
    if (!bme_ok && recoveryIsDue(letzter_bme_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_bme_recovery_ms = nowMs; bme_ok = initBme();
    }
    if (!lux_ok && recoveryIsDue(letzter_lux_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_lux_recovery_ms = nowMs; lux_ok = veml.begin();
        if (lux_ok) konfVeml();
    }
    if (!ens_ok && recoveryIsDue(letzter_ens_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_ens_recovery_ms = nowMs; ens_ok = initEns();
        if (ens_ok) ens160->setMode(ENS160_OPMODE_STD);
    }

    // BME680 lesen
    if (bme_ok && bme680.performReading()) {
        float t = bme680.temperature, h = bme680.humidity, p = bme680.pressure;
        uint32_t g = bme680.gas_resistance;
        if (isfinite(t) && isfinite(h) && isfinite(p) && h >= 0 && h <= 100 && p >= 30000 && p <= 110000) {
            temp_01c = (int16_t)lroundf(t * 10.0f);
            hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
            pressure_pa = (uint32_t)lroundf(p);
            if (bme680_gueltige_messungen < 255) bme680_gueltige_messungen++;
            gas_ohm = (gasWarmupOk(nowMs) && g > 0) ? g : GAS_OHM_UNGUELTIG;
        } else { bme_ok = false; logf("WARN", "BME680 unplausibel"); }
    }

    // VEML7700 lesen
    if (lux_ok) {
        float l = veml.readLux();
        if (!isnan(l) && l >= 0) lux = clampToU16((long)lroundf(l));
        else { lux_ok = false; logf("WARN", "VEML7700 read fail"); }
    }

    // ENS160 Kompensation schreiben
    if (ens_ok && ens160 && bme_ok) {
        int r = writeEnsEnv(ens160_adresse, bme680.temperature, bme680.humidity);
        if (r != 0) logf("WARN", "ENS160 comp fail err=%d", r);
    }

    // ENS160 Messwerte lesen
    if (ens_ok && ens160 && ens160->measure(false)) {
        uint16_t aq5 = ens160->getAQI500(), aq = ens160->getAQI();
        uint16_t maq = (aq5 > 0 && aq5 <= 500) ? aq5 : mapAqi500(aq);
        if (maq > 0) {
            aqi = maq; tvoc_ppb = ens160->getTVOC();
            eco2_ppm = ens160->geteCO2(); letzter_ens_gueltig_ms = nowMs;
        }
    }

    // Stale-Detection
    if (ensStale(nowMs)) { aqi = AIR_METRIC_UNGUELTIG; tvoc_ppb = AIR_METRIC_UNGUELTIG; eco2_ppm = AIR_METRIC_UNGUELTIG; }

    // Status aktualisieren
    runtime.fault = !(bme_ok && lux_ok) || !ens_ok;

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
    SmartHome::ExtendedRelayComfortGasConfigStateReportPayload* p =
        static_cast<SmartHome::ExtendedRelayComfortGasConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        cpy(p->node_id, sizeof(p->node_id), DEVICE_ID);
        p->relay_1 = runtime.relay_1 ? 1U : 0U;
        p->temp_01c = temp_01c;
        p->hum_01pct = hum_01pct;
        p->lux = lux;
        p->pressure_pa = pressure_pa;
        p->gas_ohm = gas_ohm;
        p->aqi = aqi;
        p->tvoc_ppb = tvoc_ppb;
        p->eco2_ppm = eco2_ppm;
        p->motion = runtime.motion_aktiv ? 1U : 0U;
        p->auto_flags = netErlDeviceBuildAutoFlags();
        p->fault = runtime.fault ? 1U : 0U;
        p->report_interval_s = (uint16_t)runtime.report_interval_s;
        p->auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    }
    if (size != nullptr) *size = sizeof(SmartHome::ExtendedRelayComfortGasConfigStateReportPayload);
}

uint8_t netErlDeviceBuildAutoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (lux_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    return f;
}

bool netErlDeviceHasSensorFault() {
    return !(bme_ok && lux_ok) || !ens_ok;
}

void netErlDeviceLogSnapshot() {
    logf("INFO", "snap t=%d h=%u l=%u p=%lu g=%lu a=%u tv=%u ec=%u m=%s r=%s",
        (int)temp_01c, hum_01pct, lux,
        (unsigned long)pressure_pa, (unsigned long)gas_ohm,
        aqi, tvoc_ppb, eco2_ppm,
        runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0");
}

bool netErlDeviceReadButton() {
#if BUTTON_1_ACTIVE_LOW
    return digitalRead(PIN_BUTTON_1) == LOW;
#else
    return digitalRead(PIN_BUTTON_1) == HIGH;
#endif
}

// =============================================================================
// RUNTIME (Baukasten Block 3 – liefert setup() und loop())
// =============================================================================
#include "../../basetypes/net_erl/NetErlRuntime.h"
