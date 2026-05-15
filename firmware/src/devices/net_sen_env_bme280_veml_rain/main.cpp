// =============================================================================
// main.cpp – NET-SEN Env BME280+VEML+Rain
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_env_bme280_veml_rain/main.cpp
// Hardware:   ESP32-C3 + BME280 + VEML7700 + digitaler Regensensor
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung (siehe PinConfig.h):
//   I2C SDA:       GPIO0
//   I2C SCL:       GPIO1
//   BME280:        Adresse 0x76 (Fallback 0x77)
//   VEML7700:      Adresse 0x10
//   Regensensor:   GPIO3, digital, active-LOW, Pullup
//   Setup-Button:  GPIO2
//   Setup-LED:     GPIO7
//
// Funktionsweise:
//   Sensor-Poll alle 2500ms. BME280 liefert Temp/Feuchte/Druck.
//   VEML7700 liefert Lux. Digitaler Regensensor (nass/trocken) via Event.
//   Auto-Recovery bei Sensor-Fehlern (alle 30s).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
#include "SensorUtils.h"

using SmartHome::clampToU16;
using SmartHome::clampHum01pct;
using SmartHome::absDiffU16;
using SmartHome::absDiffI16;
using SmartHome::valueChangedSignificantU32;
using SmartHome::recoveryIsDue;

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(int16_t*, uint16_t*, uint16_t*, uint8_t*, bool*);
void netSenDeviceExtendedStateInit();
bool netSenDeviceExtendedStatePoll(uint32_t*, uint32_t*, uint16_t*, uint16_t*, uint16_t*);
bool netSenDevicePollEvent(uint8_t*, uint8_t*, uint8_t*, uint16_t*);

#include "../../basetypes/net_sen/NetSenRuntime.h"

static_assert(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN >= 0,
    "net_sen_env_bme280_veml_rain braucht einen gueltigen Regen-Pin.");

// =============================================================================
// KONSTANTEN + LOKALER ZUSTAND
// =============================================================================

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
constexpr unsigned long SENSOR_RECOVERY_RETRY_INTERVAL_MS = 30000UL;

struct ErweiterterState {
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
};

Adafruit_BME280 sensorBme280;
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();

bool bme280Bereit = false;
bool veml7700Bereit = false;
bool regenNass = false;             // Aktueller Regen-Status
bool regenEventOffen = false;       // true = Event noch nicht gesendet
uint8_t regenEventStatus = 0U;      // Gemerkter Event-Status (1=nass)
uint8_t bme280Adresse = 0U;

unsigned long bootMs = 0UL;
unsigned long letzterSensorPollMs = 0UL;
unsigned long letzterBmeFehlerLogMs = 0UL;
unsigned long letzterVemlFehlerLogMs = 0UL;
unsigned long letzterSnapshotLogMs = 0UL;
unsigned long letzterBmeRecoveryMs = 0UL;
unsigned long letzterVemlRecoveryMs = 0UL;
unsigned long veml7700BereitSeitMs = 0UL;

ErweiterterState erweiterterState = {
    NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG};
bool erweiterterStateGeaendert = true;

bool uebernehmeWertU32(uint32_t* z, uint32_t n, uint32_t inv, uint32_t d) {
    if (!z) return false;
    const bool c = valueChangedSignificantU32(*z, n, inv, d);
    *z = n; return c;
}

void logBmeFehler(unsigned long j, const char* g) {
    if ((j - letzterBmeFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "BME280 FEHLER: %s", g);
    letzterBmeFehlerLogMs = j;
}

void logVemlFehler(unsigned long j, const char* g) {
    if ((j - letzterVemlFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "VEML7700 FEHLER: %s", g);
    letzterVemlFehlerLogMs = j;
}

bool initialisiereBme280() {
    const uint8_t addrs[] = {
        (uint8_t)NET_SEN_ENV_BME280_PRIMARY_ADDRESS,
        (uint8_t)NET_SEN_ENV_BME280_FALLBACK_ADDRESS};
    for (uint8_t a : addrs) {
        if (!sensorBme280.begin(a, &Wire)) continue;
        bme280Adresse = a;
        return true;
    }
    return false;
}

void konfiguriereVeml7700() {
    sensorVeml7700.setGain(VEML7700_GAIN_1);
    sensorVeml7700.setIntegrationTime(VEML7700_IT_100MS);
}

bool initialisiereVeml7700(unsigned long jetzt) {
    if (!sensorVeml7700.begin()) return false;
    konfiguriereVeml7700();
    veml7700BereitSeitMs = jetzt;
    return true;
}

void versucheBmeRecovery(unsigned long jetzt) {
    if (bme280Bereit || !recoveryIsDue(letzterBmeRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterBmeRecoveryMs = jetzt;
    bme280Bereit = initialisiereBme280();
    logf(bme280Bereit ? "INFO" : "WARN",
         bme280Bereit ? "BME280 Recovery ok" : "BME280 Recovery fehlgeschlagen");
}

void versucheVemlRecovery(unsigned long jetzt) {
    if (veml7700Bereit || !recoveryIsDue(letzterVemlRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterVemlRecoveryMs = jetzt;
    veml7700Bereit = initialisiereVeml7700(jetzt);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 Recovery ok" : "VEML7700 Recovery fehlgeschlagen");
}

bool leseRegenNass() {
    const int p = digitalRead(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN);
#if NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW
    return p == LOW;
#else
    return p == HIGH;
#endif
}
}  // namespace

// =============================================================================
// CUSTOM-HOOKS
// =============================================================================

void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterBmeFehlerLogMs = 0UL;
    letzterVemlFehlerLogMs = 0UL;
    letzterBmeRecoveryMs = 0UL;
    letzterVemlRecoveryMs = 0UL;
    regenEventOffen = false;

    erweiterterState = {NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
                        NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
                        NET_SEN_AIR_METRIC_UNGUELTIG};
    erweiterterStateGeaendert = true;

    Wire.setClock(I2C_CLOCK_HZ);

    bme280Bereit = initialisiereBme280();
    if (!bme280Bereit) logf("WARN", "BME280 nicht gefunden (0x%02X/0x%02X)",
         NET_SEN_ENV_BME280_PRIMARY_ADDRESS, NET_SEN_ENV_BME280_FALLBACK_ADDRESS);

    veml7700Bereit = initialisiereVeml7700(bootMs);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 init OK" : "VEML7700 nicht gefunden");

#if NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT_PULLUP);
#else
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT);
#endif
    regenNass = leseRegenNass();
    logf("INFO", "Regensensor init: pin=%d status=%s",
         NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN,
         regenNass ? "nass" : "trocken");
}

void netSenDeviceExtendedStateInit() {}

bool netSenDeviceExtendedStatePoll(
    uint32_t* pp, uint32_t* go, uint16_t* a, uint16_t* t, uint16_t* e) {
    if (!pp || !go || !a || !t || !e) return false;
    *pp = erweiterterState.pressure_pa;
    *go = erweiterterState.gas_ohm;
    *a = erweiterterState.aqi;
    *t = erweiterterState.tvoc_ppb;
    *e = erweiterterState.eco2_ppm;
    const bool c = erweiterterStateGeaendert;
    erweiterterStateGeaendert = false;
    return c;
}

bool netSenDevicePollEvent(uint8_t* et, uint8_t* tr, uint8_t* p1, uint16_t* p2) {
    if (!regenEventOffen) return false;
    regenEventOffen = false;
    if (et) *et = SH_EVENT_RAIN_DETECTED;
    if (tr) *tr = SH_TRIGGER_AUTO;
    if (p1) *p1 = regenEventStatus;
    if (p2) *p2 = 0U;
    return true;
}

bool netSenDeviceSensorPoll(
    int16_t* temp_01c, uint16_t* hum_01pct,
    uint16_t* lux, uint8_t* motion, bool* fault)
{
    if (!temp_01c || !hum_01pct || !lux || !motion || !fault) return false;

    const unsigned long jetzt = millis();
    if ((jetzt - letzterSensorPollMs) < NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS)
        return false;
    letzterSensorPollMs = jetzt;

    const int16_t vT = *temp_01c;
    const uint16_t vH = *hum_01pct;
    const uint16_t vL = *lux;
    const uint8_t vM = *motion;
    const bool vF = *fault;

    int16_t nT = vT; uint16_t nH = vH; uint16_t nL = vL;
    uint32_t nP = NET_SEN_PRESSURE_UNGUELTIG;
    bool bmeOk = false, vemlOk = false;

    versucheBmeRecovery(jetzt);
    versucheVemlRecovery(jetzt);

    // BME280-Messung
    if (bme280Bereit) {
        const float t = sensorBme280.readTemperature();
        const float h = sensorBme280.readHumidity();
        const float p = sensorBme280.readPressure();
        const bool gueltig = isfinite(t) && isfinite(h) && isfinite(p) &&
            h >= 0.0f && h <= 100.0f && p >= 30000.0f && p <= 110000.0f;
        if (gueltig) {
            nT = (int16_t)lroundf(t * 10.0f);
            nH = clampHum01pct((long)lroundf(h * 10.0f));
            nP = (uint32_t)lroundf(p);
            bmeOk = true;
        } else {
            bme280Bereit = false;
            logBmeFehler(jetzt, "Messwerte unplausibel");
        }
    } else logBmeFehler(jetzt, "Sensor nicht initialisiert");

    // VEML7700-Messung
    const bool warmup = veml7700Bereit &&
        (jetzt - veml7700BereitSeitMs) < NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS;
    if (veml7700Bereit && !warmup) {
        const float l = sensorVeml7700.readLux();
        if (isfinite(l) && l >= 0.0f) { nL = clampToU16((long)lroundf(l)); vemlOk = true; }
        else { veml7700Bereit = false; logVemlFehler(jetzt, "Lux unplausibel"); }
    } else if (!veml7700Bereit) logVemlFehler(jetzt, "Sensor nicht initialisiert");

    // Regen-Digital-Eingang
    const bool nRegen = leseRegenNass();
    if (nRegen != regenNass) {
        regenNass = nRegen;
        regenEventStatus = regenNass ? 1U : 0U;
        regenEventOffen = true;
        logf("INFO", "Regensensor Status: %s", regenNass ? "nass" : "trocken");
    }

    // Extended State (pressure)
    const bool extGeaendert = uebernehmeWertU32(
        &erweiterterState.pressure_pa, nP, NET_SEN_PRESSURE_UNGUELTIG,
        NET_SEN_ENV_BME280_VEML_RAIN_PRESSURE_DELTA_PA);
    if (extGeaendert) erweiterterStateGeaendert = true;

    const bool nF = !bmeOk || (!vemlOk && !warmup);

    *temp_01c = nT; *hum_01pct = nH; *lux = nL;
    *motion = 0U; *fault = nF;

    if ((jetzt - letzterSnapshotLogMs) >= NET_SEN_ENV_BME280_VEML_RAIN_SNAPSHOT_LOG_INTERVAL_MS) {
        logf("INFO", "snapshot t=%d h=%u l=%u p=%lu r=%s f=%s",
             nT, nH, nL, (unsigned long)erweiterterState.pressure_pa,
             regenNass ? "nass" : "trocken", nF ? "true" : "false");
        letzterSnapshotLogMs = jetzt;
    }

    return absDiffI16(nT, vT) >= NET_SEN_ENV_BME280_VEML_RAIN_TEMP_DELTA_01C ||
           absDiffU16(nH, vH) >= NET_SEN_ENV_BME280_VEML_RAIN_HUM_DELTA_01PCT ||
           absDiffU16(nL, vL) >= NET_SEN_ENV_BME280_VEML_RAIN_LUX_DELTA ||
           vM != 0U || nF != vF;
}
