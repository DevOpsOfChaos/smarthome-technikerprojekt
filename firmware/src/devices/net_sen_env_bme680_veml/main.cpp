/**
 * @file main.cpp
 * @brief NET-SEN Env: Umweltsensor mit BME680 + VEML7700 + ENS160
 *
 * @details Erweiterter net_sen-Sensor-Adapter. BME680 liefert Temp/Feuchte/Druck/Rohgas,
 *          VEML7700 liefert Lux, ENS160 liefert AQI/TVOC/eCO2 mit BME680-Kompensation.
 *          BME680-Heizprofil: 320°C, 150ms. Gas erst nach 180s Warmup + 5 Messungen belastbar.
 *          ENS160 stale-detection: 120s ohne gueltige Daten → Werte auf UNGUELTIG.
 *          Gedrosseltes Fehler-Logging: BME/VEML-Fehler nur alle 15s.
 *
 * Hardware:   ESP32-C3 + BME680 (0x76/0x77) + VEML7700 (0x10) + ENS160 (0x52/0x53)
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <ScioSense_ENS160.h>

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
#include "SensorUtils.h"

using SmartHome::clampToU16;
using SmartHome::clampHum01pct;
using SmartHome::absDiffU16;
using SmartHome::absDiffI16;
using SmartHome::valueChangedSignificantU32;
using SmartHome::updateAndCheckU32;
using SmartHome::recoveryIsDue;
using SmartHome::sensorValueStale;
using SmartHome::gasWarmupComplete;

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

/**
 * @brief Prüft, ob der BME680-Gassensor ausreichend warmgelaufen ist.
 *
 * @param j Aktueller Zeitstempel in ms (millis()).
 * @return true wenn Warmup-Zeit und Mindest-Anzahl Messungen erreicht sind.
 */
bool gasWarmupOk(unsigned long j) {
    return gasWarmupComplete(bootMs, j, NET_SEN_ENV_BME680_GAS_WARMUP_MS, bmeReads, NET_SEN_ENV_BME680_GAS_WARMUP_MIN_READS);
}

/**
 * @brief Prüft, ob die ENS160-Warmup-Phase abgeschlossen ist.
 *
 * @param j Aktueller Zeitstempel in ms (millis()).
 * @return true wenn die ENS160-Warmup-Zeit seit Boot überschritten wurde.
 */
bool ensWarmupOk(unsigned long j) {
    return (j - bootMs) >= NET_SEN_ENV_ENS160_WARMUP_MS;
}

/**
 * @brief Prüft, ob die ENS160-Messwerte veraltet (stale) sind.
 *
 * @details Nur relevant nach Warmup. Verwendet sensorValueStale() mit
 *          konfigurierbarem Timeout für die Erkennung veralteter Daten.
 *
 * @param j Aktueller Zeitstempel in ms (millis()).
 * @return true wenn ENS160-Daten als veraltet gelten.
 */
bool ensStale(unsigned long j) {
    if (!ensWarmupOk(j)) return false;
    return sensorValueStale(ensOk && (ens != nullptr), lastEnsValid, j, NET_SEN_ENV_ENS160_STALE_TIMEOUT_MS);
}

/**
 * @brief Mappt ENS160-AQI (1-5) auf die Skala 0-500.
 *
 * @details Werte ausserhalb 1-5 werden als ungueltig (0) behandelt.
 *
 * @param r Roher AQI-Wert (1-5).
 * @return Gemappter Wert (0, 100, 200, 300, 400 oder 500).
 */
uint16_t mapAqi500(uint16_t r) { return (r >= 1U && r <= ENS160_AQI_MAX_BASIC) ? (uint16_t)(r * 100U) : 0U; }

/**
 * @brief Kodiert Temperatur in Grad Celsius als uint16 (Kelvin * 64).
 *
 * @param c Temperatur in °C.
 * @return Kodierter uint16-Wert.
 */
uint16_t encTemp(float c) { return (uint16_t)((c + 273.15f) * 64.0f); }

/**
 * @brief Kodiert relative Luftfeuchte als uint16 (Prozent * 512).
 *
 * @param h Relative Feuchte in % (0-100).
 * @return Kodierter uint16-Wert.
 */
uint16_t encHum(float h) { return (uint16_t)(h * 512.0f); }

/**
 * @brief Schreibt Temperatur- und Feuchte-Kompensationsdaten an den ENS160.
 *
 * @param addr I2C-Adresse des ENS160.
 * @param t    Temperatur in °C.
 * @param h    Relative Feuchte in %.
 * @return Wire.endTransmission()-Ergebniscode (0 = Erfolg).
 */
int writeEnsEnv(uint8_t addr, float t, float h) {
    uint8_t buf[4]; uint16_t te = encTemp(t), he = encHum(h);
    buf[0] = te & 0xFF; buf[1] = (te >> 8) & 0xFF;
    buf[2] = he & 0xFF; buf[3] = (he >> 8) & 0xFF;
    Wire.beginTransmission(addr); Wire.write(ENS160_REG_TEMP_IN);
    Wire.write(buf, 4); return Wire.endTransmission();
}

/**
 * @brief Gedrosseltes Fehler-Logging für den BME680 (max. alle 15s).
 *
 * @param j Aktueller Zeitstempel in ms.
 * @param g Log-Nachricht.
 */
void logBme(unsigned long j, const char* g) { if ((j - lastBmeLog) < 15000UL) return; logf("WARN", "BME: %s", g); lastBmeLog = j; }

/**
 * @brief Gedrosseltes Fehler-Logging für den VEML7700 (max. alle 15s).
 *
 * @param j Aktueller Zeitstempel in ms.
 * @param g Log-Nachricht.
 */
void logVeml(unsigned long j, const char* g) { if ((j - lastVemlLog) < 15000UL) return; logf("WARN", "VEML: %s", g); lastVemlLog = j; }

/**
 * @brief Initialisiert den BME680 mit Fallback-Adresse.
 *
 * @details Probiert zuerst PRIMARY_ADDRESS, dann FALLBACK_ADDRESS.
 *          Konfiguriert Oversampling, IIR-Filter und Gas-Heizprofil (320°C, 150ms).
 *
 * @return true bei erfolgreicher Initialisierung.
 */
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

/**
 * @brief Initialisiert den ENS160 mit Fallback-Adresse.
 *
 * @details Probiert zuerst ens52 (PRIMARY_ADDRESS), dann ens53 (FALLBACK_ADDRESS).
 *          Setzt den ens-Pointer auf das erfolgreiche Objekt.
 *
 * @return true wenn einer der ENS160-Chips antwortet.
 */
bool initEns() {
    ens = &ens52;
    if (ens->begin()) return true;
    ens = &ens53;
    if (ens->begin()) return true;
    ens = nullptr; return false;
}

/**
 * @brief Versetzt den ENS160 in den Standard-Betriebsmodus (STD).
 *
 * @details Bei Fehlschlag wird der ens-Pointer auf nullptr gesetzt,
 *          um Folgeleseversuche zu verhindern.
 */
void setEnsActive() {
    if (!ens || !ens->setMode(ENS160_OPMODE_STD)) { ens = nullptr; return; }
    logf("INFO", "ENS160 init OK");
}

/**
 * @brief Aktualisiert die ENS160-Umgebungskompensation (Temp + Feuchte).
 *
 * @param bv true wenn gueltige BME680-Werte vorliegen.
 * @param t  Temperatur in °C.
 * @param h  Relative Feuchte in %.
 */
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

/**
 * @brief Initialisiert alle Umweltsensoren (BME680, VEML7700, ENS160).
 *
 * @details Setzt Boot-Zeitstempel, Zustandsvariablen und I2C-Clock.
 *          Initialisiert die drei Sensoren nacheinander mit Fallback-Logik.
 */
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

/**
 * @brief Initialisiert erweiterte Zustandsdaten (derzeit leer).
 *
 * @details Reserviert für zukünftige Erweiterungen des Extended-State-Systems.
 */
void netSenDeviceExtendedStateInit() {}

/**
 * @brief Liefert erweiterte Sensordaten (Druck, Gas, AQI, TVOC, eCO2) zurueck.
 *
 * @param[out] pp Zeiger auf Luftdruck in Pa.
 * @param[out] go Zeiger auf Gaswiderstand in Ohm.
 * @param[out] a  Zeiger auf AQI (0-500).
 * @param[out] t  Zeiger auf TVOC in ppb.
 * @param[out] e  Zeiger auf eCO2 in ppm.
 * @return true wenn sich Werte seit letztem Poll geaendert haben.
 */
bool netSenDeviceExtendedStatePoll(uint32_t* pp, uint32_t* go, uint16_t* a, uint16_t* t, uint16_t* e) {
    if (!pp || !go || !a || !t || !e) return false;
    *pp = erwState.pp; *go = erwState.go; *a = erwState.aqi; *t = erwState.tvoc; *e = erwState.eco2;
    bool c = erwChanged; erwChanged = false; return c;
}

/**
 * @brief Pollt alle Umweltsensoren und liefert Basiswerte zurueck.
 *
 * @details Zentrale Poll-Funktion. Liest BME680 (T/H/P/Gas), VEML7700 (Lux)
 *          und ENS160 (AQI/TVOC/eCO2). Fuehrt ENS160-Umgebungskompensation
 *          mit BME680-Daten durch. Erkennt stale ENS160-Daten. Berechnet
 *          Hysterese fuer Extended-State-Ausgaben und schreibt Snapshot-Logs.
 *
 * @param[out] t01c Temperatur in 0.1 °C.
 * @param[out] h01p Relative Feuchte in 0.1 %.
 * @param[out] lux  Beleuchtungsstaerke in Lux.
 * @param[out] mot  Motion-Flag (derzeit immer 0).
 * @param[out] flt  Fehler-Flag (true = Sensorwerte unzuverlaessig).
 * @return true wenn sich Basiswerte signifikant geaendert haben.
 */
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
            nT = (int16_t)lroundf(bmeT * 10.0f); nH = clampHum01pct((long)lroundf(bmeH * 10.0f));
            nP = (uint32_t)lroundf(p); bv = true;
            if (bmeReads < 255) bmeReads++;
            if (gasWarmupOk(j) && g > 0) { nG = g; if (!gasWarmupLogged) { logf("INFO", "Gas warmup done"); gasWarmupLogged = true; } }
        } else logBme(j, "ungueltig");
    } else logBme(j, "read fail");

    // VEML7700 lesen
    if (vemlOk && (j - bootMs) >= NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS) {
        float l = sensorVeml.readLux();
        if (isfinite(l) && l >= 0) { nL = clampToU16((long)lroundf(l)); vv = true; }
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
    bool eChg = updateAndCheckU32(&erwState.pp, nP, NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_ENV_BME680_VEML_PRESSURE_DELTA_PA);
    eChg = updateAndCheckU32(&erwState.go, nG, NET_SEN_GAS_OHM_UNGUELTIG, NET_SEN_ENV_BME680_VEML_GAS_DELTA_OHM) || eChg;
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
    return absDiffI16(nT, vT) >= NET_SEN_ENV_BME680_VEML_TEMP_DELTA_01C ||
           absDiffU16(nH, vH) >= NET_SEN_ENV_BME680_VEML_HUM_DELTA_01PCT ||
           absDiffU16(nL, vL) >= NET_SEN_ENV_BME680_VEML_LUX_DELTA || vM != 0U || nF != vF;
}
