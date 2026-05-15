// =============================================================================
// main.cpp – NET-SEN Env BME680+VEML+ENS160 (Umweltsensor)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_env_bme680_veml/main.cpp
// Hardware:   ESP32-C3 + BME680 + VEML7700 + ENS160
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA:  GPIO0
//   I2C SCL:  GPIO1
//   BME680:   0x76 (Temp/Feuchte/Druck/Rohgas)
//   VEML7700: 0x10 (Lux)
//   ENS160:   0x52 (AQI/TVOC/eCO2, mit BME680-Kompensation)
//
// Funktionsweise:
//   Sensor-Poll alle 2500ms. BME680 mit Heizprofil (320 Grad, 150ms).
//   ENS160-Kompensation via BME680-Temperatur/Feuchte.
//   gas_ohm erst nach 180s Warmup + 5 Messungen belastbar.
//   ENS160 stale-detection: 120s ohne gueltige Daten.
//   Snapshot-Log alle 30s.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <ScioSense_ENS160.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#ifndef ENS160_REG_TEMP_IN
#define ENS160_REG_TEMP_IN 0x13
#endif

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(int16_t*, uint16_t*, uint16_t*, uint8_t*, bool*);
void netSenDeviceExtendedStateInit();
bool netSenDeviceExtendedStatePoll(uint32_t*, uint32_t*, uint16_t*, uint16_t*, uint16_t*);

#include "../../basetypes/net_sen/NetSenRuntime.h"

// =============================================================================
// KONSTANTEN + ZUSTAND
// =============================================================================

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
constexpr unsigned long SENSOR_SNAPSHOT_LOG_INTERVAL_MS = 30000UL;
constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;

struct ErwState { uint32_t pp; uint32_t go; uint16_t aqi; uint16_t tvoc; uint16_t eco2; };
struct ErwState last = {0};

Adafruit_BME680 sensorBme680;
Adafruit_VEML7700 sensorVeml = Adafruit_VEML7700();
ScioSense_ENS160 ens52(NET_SEN_ENV_ENS160_PRIMARY_ADDRESS);
ScioSense_ENS160 ens53(NET_SEN_ENV_ENS160_FALLBACK_ADDRESS);
ScioSense_ENS160* ens = nullptr;

bool bmeOk = false, vemlOk = false, ensOk = false;
unsigned long bootMs = 0UL, lastPoll = 0UL, lastBmeLog = 0UL, lastVemlLog = 0UL;
unsigned long lastEnsInfo = 0UL, lastEnsErr = 0UL, lastEnsValid = 0UL, lastSnap = 0UL;
uint8_t bmeReads = 0U;
bool gasWarmupLogged = false, ensWaitLogged = false;
bool ensCompActive = false, ensCompCalled = false;
int ensCompResult = -1;
ErwState erwState = {NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
                     NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
                     NET_SEN_AIR_METRIC_UNGUELTIG};
bool erwChanged = true;

// -- Hilfsfunktionen --
// c16 – Wert auf uint16-Bereich begrenzen (0-65535)
uint16_t c16(long v) { return v < 0L ? 0U : v > 65535L ? 65535U : (uint16_t)v; }
// cH – Wert auf 0.1%-Feuchtebereich begrenzen (0-1000)
uint16_t cH(long v) { return v < 0L ? 0U : v > 1000L ? 1000U : (uint16_t)v; }
// ad16 – Absolute Differenz zweier uint16-Werte
uint16_t ad16(uint16_t a, uint16_t b) { return a > b ? a - b : b - a; }
// adI16 – Absolute Differenz zweier int16-Werte
int16_t adI16(int16_t a, int16_t b) { return a > b ? a - b : b - a; }
// chkU32 – Prueft ob sich Wert n gegenueber alt a signifikant geaendert hat (>= delta)
bool chkU32(uint32_t a, uint32_t n, uint32_t inv, uint32_t d) {
    if (a == n) return false;
    if (a == inv || n == inv) return true;
    return (a > n ? a - n : n - a) >= d;
}
// setU32 – Wert setzen + Aenderung erkennen (analog chkU32, schreibt direkt)
bool setU32(uint32_t* z, uint32_t n, uint32_t inv, uint32_t d) {
    if (!z) return false; bool c = chkU32(*z, n, inv, d); *z = n; return c;
}

// gasWarmupOk – BME680-Gassensor ausreichend warmgelaufen?
bool gasWarmupOk(unsigned long j) {
    return (j - bootMs) >= NET_SEN_ENV_BME680_GAS_WARMUP_MS && bmeReads >= NET_SEN_ENV_BME680_GAS_WARMUP_MIN_READS;
}
// ensWarmupOk – ENS160-Warmup-Phase abgeschlossen?
bool ensWarmupOk(unsigned long j) { return (j - bootMs) >= NET_SEN_ENV_ENS160_WARMUP_MS; }
// ensStale – ENS160-Messwerte veraltet?
bool ensStale(unsigned long j) {
    if (!ensWarmupOk(j)) return false;
    if (!ensOk || !ens) return true;
    return lastEnsValid == 0UL || (j - lastEnsValid) > NET_SEN_ENV_ENS160_STALE_TIMEOUT_MS;
}
// mapAqi500 – ENS160-AQI (1-5) auf Skala 0-500 abbilden (0 = ungueltig)
uint16_t mapAqi500(uint16_t r) { return (r >= 1U && r <= ENS160_AQI_MAX_BASIC) ? (uint16_t)(r * 100U) : 0U; }
// encTemp – Temperatur in Kelvin * 64 als uint16 kodieren
uint16_t encTemp(float c) { return (uint16_t)((c + 273.15f) * 64.0f); }
// encHum – Relative Feuchte * 512 als uint16 kodieren
uint16_t encHum(float h) { return (uint16_t)(h * 512.0f); }

int writeEnsEnv(uint8_t addr, float t, float h) {
    uint8_t buf[4]; uint16_t te = encTemp(t), he = encHum(h);
    buf[0] = te & 0xFF; buf[1] = (te >> 8) & 0xFF;
    buf[2] = he & 0xFF; buf[3] = (he >> 8) & 0xFF;
    Wire.beginTransmission(addr); Wire.write(ENS160_REG_TEMP_IN);
    Wire.write(buf, 4); return Wire.endTransmission();
}

void logBme(unsigned long j, const char* g) { if ((j - lastBmeLog) < 15000UL) return; logf("WARN", "BME: %s", g); lastBmeLog = j; }
void logVeml(unsigned long j, const char* g) { if ((j - lastVemlLog) < 15000UL) return; logf("WARN", "VEML: %s", g); lastVemlLog = j; }

bool initialisiereBme680() {
    const uint8_t addrs[] = {(uint8_t)NET_SEN_ENV_BME680_PRIMARY_ADDRESS,
                             (uint8_t)NET_SEN_ENV_BME680_FALLBACK_ADDRESS};
    for (uint8_t a : addrs) {
        if (!sensorBme680.begin(a, &Wire)) continue;
        sensorBme680.setTemperatureOversampling(BME680_OS_8X);
        sensorBme680.setHumidityOversampling(BME680_OS_2X);
        sensorBme680.setPressureOversampling(BME680_OS_4X);
        sensorBme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
        sensorBme680.setGasHeater(320U, 150U);
        logf("INFO", "BME680 init OK auf 0x%02X", a); return true;
    }
    return false;
}

bool initEns() {
    ens = &ens52;
    if (ens->begin()) return true;
    ens = &ens53;
    if (ens->begin()) return true;
    ens = nullptr; return false;
}

void setEnsActive() {
    if (!ens || !ens->setMode(ENS160_OPMODE_STD)) { ens = nullptr; return; }
    logf("INFO", "ENS160 init OK");
}

void updEnsComp(bool bv, float t, float h) {
    ensCompActive = false; ensCompCalled = false; ensCompResult = -1;
    if (!ensOk || !ens || !bv) return;
    ensCompCalled = true;
    ensCompResult = writeEnsEnv(ens == &ens52 ? NET_SEN_ENV_ENS160_PRIMARY_ADDRESS : NET_SEN_ENV_ENS160_FALLBACK_ADDRESS, t, h);
    ensCompActive = (ensCompResult == 0);
}
}  // namespace

// =============================================================================
// CUSTOM-HOOKS
// =============================================================================

void netSenDeviceSensorInit() {
    bootMs = millis(); lastPoll = 0UL;
    lastBmeLog = 0UL; lastVemlLog = 0UL; lastEnsInfo = 0UL; lastEnsErr = 0UL;
    lastEnsValid = 0UL; lastSnap = 0UL; bmeReads = 0U;
    gasWarmupLogged = false; ensWaitLogged = false;
    ensCompActive = false; ensCompCalled = false; ensCompResult = -1;
    erwState = {NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
                NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
                NET_SEN_AIR_METRIC_UNGUELTIG};
    erwChanged = true;
    Wire.setClock(I2C_CLOCK_HZ);

    bmeOk = initialisiereBme680();
    if (!bmeOk) logf("WARN", "BME680 nicht gefunden");

    if (!sensorVeml.begin()) { vemlOk = false; logf("WARN", "VEML7700 nicht gefunden"); }
    else { vemlOk = true; sensorVeml.setGain(VEML7700_GAIN_1); sensorVeml.setIntegrationTime(VEML7700_IT_400MS); }

    ensOk = initEns();
    if (!ensOk) logf("WARN", "ENS160 nicht gefunden");
    else setEnsActive();
}

void netSenDeviceExtendedStateInit() {}

bool netSenDeviceExtendedStatePoll(uint32_t* pp, uint32_t* go, uint16_t* a, uint16_t* t, uint16_t* e) {
    if (!pp || !go || !a || !t || !e) return false;
    *pp = erwState.pp; *go = erwState.go; *a = erwState.aqi; *t = erwState.tvoc; *e = erwState.eco2;
    bool c = erwChanged; erwChanged = false; return c;
}

bool netSenDeviceSensorPoll(int16_t* t01c, uint16_t* h01p, uint16_t* lux, uint8_t* mot, bool* flt) {
    if (!t01c || !h01p || !lux || !mot || !flt) return false;
    unsigned long j = millis();
    if ((j - lastPoll) < NET_SEN_ENV_BME680_VEML_SENSOR_READ_INTERVAL_MS) return false;
    lastPoll = j;

    int16_t vT = *t01c; uint16_t vH = *h01p, vL = *lux; uint8_t vM = *mot; bool vF = *flt;
    int16_t nT = vT; uint16_t nH = vH, nL = vL; float bmeT = NAN, bmeH = NAN;
    bool bv = false, vv = false;
    uint32_t nP = NET_SEN_PRESSURE_UNGUELTIG, nG = NET_SEN_GAS_OHM_UNGUELTIG;

    // BME680 lesen
    if (bmeOk && sensorBme680.performReading()) {
        bmeT = sensorBme680.temperature; bmeH = sensorBme680.humidity;
        float p = sensorBme680.pressure; uint32_t g = sensorBme680.gas_resistance;
        if (isfinite(bmeT) && isfinite(bmeH) && isfinite(p) && bmeH >= 0 && bmeH <= 100 && p >= 30000 && p <= 110000) {
            nT = (int16_t)lroundf(bmeT * 10.0f); nH = cH((long)lroundf(bmeH * 10.0f));
            nP = (uint32_t)lroundf(p); bv = true;
            if (bmeReads < 255) bmeReads++;
            if (gasWarmupOk(j) && g > 0) { nG = g; if (!gasWarmupLogged) { logf("INFO", "Gas warmup done"); gasWarmupLogged = true; } }
        } else logBme(j, "ungueltig");
    } else logBme(j, "read fail");

    // VEML7700 lesen
    if (vemlOk && (j - bootMs) >= NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS) {
        float l = sensorVeml.readLux();
        if (isfinite(l) && l >= 0) { nL = c16((long)lroundf(l)); vv = true; }
        else logVeml(j, "Lux unplausibel");
    } else if (!vemlOk) logVeml(j, "Sensor nicht init");

    // ENS160-Kompensation + lesen
    updEnsComp(bv, bmeT, bmeH);
    uint16_t nA = erwState.aqi, nTv = erwState.tvoc, nEc = erwState.eco2;
    bool ensValid = false;
    if (ensOk && ens && ens->measure(false)) {
        uint16_t aq5 = ens->getAQI500();
        uint16_t aq = ens->getAQI();
        uint16_t mAq = (aq5 > 0 && aq5 <= 500) ? aq5 : mapAqi500(aq);
        if (mAq > 0) { nA = mAq; nTv = ens->getTVOC(); nEc = ens->geteCO2();
            lastEnsValid = j; ensValid = true; ensWaitLogged = false; }
    }
    if (!ensValid && ensStale(j)) { nA = NET_SEN_AIR_METRIC_UNGUELTIG; nTv = NET_SEN_AIR_METRIC_UNGUELTIG; nEc = NET_SEN_AIR_METRIC_UNGUELTIG; }

    // Hysterese Extended State
    bool eChg = setU32(&erwState.pp, nP, NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_ENV_BME680_VEML_PRESSURE_DELTA_PA);
    eChg = setU32(&erwState.go, nG, NET_SEN_GAS_OHM_UNGUELTIG, NET_SEN_ENV_BME680_VEML_GAS_DELTA_OHM) || eChg;
    erwChanged = (eChg) || erwChanged;  // keep mark

    bool ef = ensStale(j);
    bool nF = !(bv && vv) || ef;
    *t01c = nT; *h01p = nH; *lux = nL; *mot = 0U; *flt = nF;

    if ((j - lastSnap) >= SENSOR_SNAPSHOT_LOG_INTERVAL_MS) {
        logf("INFO", "snap t=%d h=%u l=%u p=%lu g=%lu a=%u tv=%u ec=%u f=%s",
             nT, nH, nL, (unsigned long)erwState.pp, (unsigned long)erwState.go,
             erwState.aqi, erwState.tvoc, erwState.eco2, nF ? "true" : "false");
        lastSnap = j;
    }
    return adI16(nT, vT) >= NET_SEN_ENV_BME680_VEML_TEMP_DELTA_01C ||
           ad16(nH, vH) >= NET_SEN_ENV_BME680_VEML_HUM_DELTA_01PCT ||
           ad16(nL, vL) >= NET_SEN_ENV_BME680_VEML_LUX_DELTA || vM != 0U || nF != vF;
}
