/**
 * @file main.cpp
 * @brief NET-SEN Env: Umweltsensor mit BME280 + VEML7700 + digitalem Regensensor
 *
 * @details Erweiterter net_sen-Sensor-Adapter mit Regen-Event-Support.
 *          BME280: Temp/Feuchte/Druck (0x76, Fallback 0x77).
 *          VEML7700: Lux (0x10, 100ms Integration, 500ms Warmup).
 *          Regensensor: digitaler Eingang (GPIO3), active-LOW, Pullup.
 *          Auto-Recovery bei Sensor-Fehlern (alle 30s).
 *          Gedrosseltes Fehler-Logging.
 *
 * Hardware:   ESP32-C3 + BME280 + VEML7700 + digitaler Regensensor
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 */

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
    "net_sen_weather_station braucht einen gueltigen Regen-Pin.");

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

/**
 * @brief Übernimmt einen neuen 32-Bit-Wert mit Delta-Prüfung und meldet Änderung
 *
 * @param z  Zeiger auf den aktuellen Wert (wird bei Änderung überschrieben)
 * @param n  Neuer Wert
 * @param inv Ungültigkeitswert, der keine Änderung auslösen soll
 * @param d  Delta-Schwelle für signifikante Änderung
 * @return true wenn sich der Wert signifikant geändert hat, false sonst
 */
bool uebernehmeWertU32(uint32_t* z, uint32_t n, uint32_t inv, uint32_t d) {
    if (!z) return false;
    const bool c = valueChangedSignificantU32(*z, n, inv, d);
    *z = n; return c;
}

/**
 * @brief Gedrosseltes Fehler-Logging für BME280 (vermeidet Log-Spam bei Dauerfehlern)
 *
 * @param j Aktueller Zeitstempel in ms
 * @param g Fehlerbeschreibung als C-String
 */
void logBmeFehler(unsigned long j, const char* g) {
    if ((j - letzterBmeFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "BME280 FEHLER: %s", g);
    letzterBmeFehlerLogMs = j;
}

/**
 * @brief Gedrosseltes Fehler-Logging für VEML7700 (vermeidet Log-Spam bei Dauerfehlern)
 *
 * @param j Aktueller Zeitstempel in ms
 * @param g Fehlerbeschreibung als C-String
 */
void logVemlFehler(unsigned long j, const char* g) {
    if ((j - letzterVemlFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "VEML7700 FEHLER: %s", g);
    letzterVemlFehlerLogMs = j;
}

/**
 * @brief Initialisiert den BME280, zuerst über die primäre Adresse, bei Fehler über Fallback-Adresse
 *
 * @return true wenn der Sensor an einer der Adressen gefunden wurde, false sonst
 */
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

/**
 * @brief Konfiguriert den VEML7700: Gain x1, Integrationszeit 100ms
 */
void konfiguriereVeml7700() {
    sensorVeml7700.setGain(VEML7700_GAIN_1);
    sensorVeml7700.setIntegrationTime(VEML7700_IT_100MS);
}

/**
 * @brief Initialisiert den VEML7700, konfiguriert ihn und setzt den Bereit-Zeitstempel für die Warmup-Phase
 *
 * @param jetzt Aktueller Zeitstempel in ms (für Warmup-Referenz)
 * @return true bei erfolgreicher Initialisierung, false wenn Sensor nicht antwortet
 */
bool initialisiereVeml7700(unsigned long jetzt) {
    if (!sensorVeml7700.begin()) return false;
    konfiguriereVeml7700();
    veml7700BereitSeitMs = jetzt;
    return true;
}

/**
 * @brief Versucht BME280-Recovery, wenn der Sensor ausgefallen ist und das Wiederholintervall abgelaufen ist
 *
 * @param jetzt Aktueller Zeitstempel in ms
 */
void versucheBmeRecovery(unsigned long jetzt) {
    if (bme280Bereit || !recoveryIsDue(letzterBmeRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterBmeRecoveryMs = jetzt;
    bme280Bereit = initialisiereBme280();
    logf(bme280Bereit ? "INFO" : "WARN",
         bme280Bereit ? "BME280 Recovery ok" : "BME280 Recovery fehlgeschlagen");
}

/**
 * @brief Versucht VEML7700-Recovery, wenn der Sensor ausgefallen ist und das Wiederholintervall abgelaufen ist
 *
 * @param jetzt Aktueller Zeitstempel in ms
 */
void versucheVemlRecovery(unsigned long jetzt) {
    if (veml7700Bereit || !recoveryIsDue(letzterVemlRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterVemlRecoveryMs = jetzt;
    veml7700Bereit = initialisiereVeml7700(jetzt);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 Recovery ok" : "VEML7700 Recovery fehlgeschlagen");
}

/**
 * @brief Liest den digitalen Regensensor über den konfigurierten GPIO-Pin aus
 *
 * @return true wenn der Regensensor Nässe meldet (active-LOW: LOW = nass), false bei Trockenheit
 */
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

/**
 * @brief System-Init: richtet I2C ein, initialisiert BME280, VEML7700 und den digitalen Regen-Pin
 */
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
    regenEventStatus = regenNass ? 1U : 0U;
    regenEventOffen = true;
    logf("INFO", "Regensensor init: pin=%d status=%s",
         NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN,
         regenNass ? "nass" : "trocken");
}

/**
 * @brief Leere Initialisierung für erweiterte Zustandswerte (Druck, Gas, AQI, TVOC, eCO2)
 */
void netSenDeviceExtendedStateInit() {}

/**
 * @brief Gibt die erweiterten Zustandswerte aus und setzt das Änderungsflag zurück
 *
 * @param pp Ausgabe: Luftdruck in Pa (oder NET_SEN_PRESSURE_UNGUELTIG)
 * @param go Ausgabe: Gas-Widerstand in Ohm (oder NET_SEN_GAS_OHM_UNGUELTIG)
 * @param a  Ausgabe: Luftqualitätsindex (oder NET_SEN_AIR_METRIC_UNGUELTIG)
 * @param t  Ausgabe: TVOC in ppb (oder NET_SEN_AIR_METRIC_UNGUELTIG)
 * @param e  Ausgabe: eCO2 in ppm (oder NET_SEN_AIR_METRIC_UNGUELTIG)
 * @return true wenn sich die Werte seit dem letzten Poll geändert haben, false sonst
 */
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

/**
 * @brief Sendet ein ausstehendes Regen-Event (nass/trocken) bei Statuswechsel des digitalen Regensensors
 *
 * @param et Ausgabe: Event-Typ (SH_EVENT_RAIN_DETECTED)
 * @param tr Ausgabe: Trigger-Art (SH_TRIGGER_AUTO)
 * @param p1 Ausgabe: Regenstatus (1 = nass, 0 = trocken)
 * @param p2 Ausgabe: Zusatzwert (ungenutzt, immer 0)
 * @return true wenn ein Event ausstand und jetzt gesendet wurde, false sonst
 */
bool netSenDevicePollEvent(uint8_t* et, uint8_t* tr, uint8_t* p1, uint16_t* p2) {
    if (!regenEventOffen) return false;
    regenEventOffen = false;
    if (et) *et = SH_EVENT_RAIN_DETECTED;
    if (tr) *tr = SH_TRIGGER_AUTO;
    if (p1) *p1 = regenEventStatus;
    if (p2) *p2 = 0U;
    return true;
}

/**
 * @brief Poll-Zyklus für alle Sensoren: Recovery, BME280 (Temp/Feuchte/Druck), VEML7700 (Lux mit Warmup-Phase),
 *        digitaler Regensensor (Event-Generierung) und Extended State (Druck-Delta)
 *
 * @param temp_01c Ein/Ausgabe: Temperatur in 0.1°C (wird bei signifikanter Änderung aktualisiert)
 * @param hum_01pct Ein/Ausgabe: relative Feuchte in 0.1% (wird bei signifikanter Änderung aktualisiert)
 * @param lux       Ein/Ausgabe: Beleuchtungsstärke in Lux (wird bei signifikanter Änderung aktualisiert)
 * @param motion    Ausgabe: Bewegung (ungenutzt, immer 0)
 * @param fault     Ein/Ausgabe: Fehlerflag (true wenn BME280 oder VEML7700 nicht ok)
 * @return true wenn sich mindestens ein Messwert signifikant geändert hat, false sonst
 */
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
