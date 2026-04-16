#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault);
void netSenDeviceExtendedStateInit();
bool netSenDeviceExtendedStatePoll(
    uint32_t* pressure_pa,
    uint32_t* gas_ohm,
    uint16_t* aqi,
    uint16_t* tvoc_ppb,
    uint16_t* eco2_ppm);
bool netSenDevicePollEvent(
    uint8_t* event_type,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2);

#include "../../basetypes/net_sen/NetSenRuntime.h"

static_assert(
    NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN >= 0,
    "net_sen_env_bme280_veml_rain braucht einen gueltigen digitalen Regen-Pin.");

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
bool regenNass = false;
bool regenEventOffen = false;
uint8_t regenEventStatus = 0U;
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
    NET_SEN_PRESSURE_UNGUELTIG,
    NET_SEN_GAS_OHM_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG};
bool erweiterterStateGeaendert = true;

uint16_t clampToU16(long value) {
    if (value < 0L) return 0U;
    if (value > 65535L) return 65535U;
    return (uint16_t)value;
}

uint16_t clampHum01pct(long value) {
    if (value < 0L) return 0U;
    if (value > 1000L) return 1000U;
    return (uint16_t)value;
}

uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

uint16_t absDiffI16(int16_t a, int16_t b) {
    const int32_t diff = (int32_t)a - (int32_t)b;
    const uint32_t absDiff = diff < 0 ? (uint32_t)(-diff) : (uint32_t)diff;
    return absDiff > 65535UL ? 65535U : (uint16_t)absDiff;
}

bool wertAendertU32(uint32_t alt, uint32_t neu, uint32_t invalid, uint32_t delta) {
    if (alt == neu) return false;
    if (alt == invalid || neu == invalid) return true;
    const uint32_t diff = alt > neu ? (alt - neu) : (neu - alt);
    return diff >= delta;
}

bool uebernehmeWertU32(uint32_t* ziel, uint32_t neu, uint32_t invalid, uint32_t delta) {
    if (!ziel) return false;
    const bool changed = wertAendertU32(*ziel, neu, invalid, delta);
    *ziel = neu;
    return changed;
}

void logBmeFehlerGedrosselt(unsigned long jetztMs, const char* grund) {
    if ((jetztMs - letzterBmeFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "BME280 FEHLER: %s", grund);
    letzterBmeFehlerLogMs = jetztMs;
}

void logVemlFehlerGedrosselt(unsigned long jetztMs, const char* grund) {
    if ((jetztMs - letzterVemlFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "VEML7700 FEHLER: %s", grund);
    letzterVemlFehlerLogMs = jetztMs;
}

bool initialisiereBme280() {
    const uint8_t moeglicheAdressen[] = {
        (uint8_t)NET_SEN_ENV_BME280_PRIMARY_ADDRESS,
        (uint8_t)NET_SEN_ENV_BME280_FALLBACK_ADDRESS};

    bme280Adresse = 0U;
    for (uint8_t adresse : moeglicheAdressen) {
        if (!sensorBme280.begin(adresse, &Wire)) continue;

        bme280Adresse = adresse;
        logf("INFO", "BME280 init OK auf 0x%02X", bme280Adresse);
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

bool sensorRecoveryFaellig(unsigned long letzterVersuchMs, unsigned long jetzt) {
    return letzterVersuchMs == 0UL ||
           (jetzt - letzterVersuchMs) >= SENSOR_RECOVERY_RETRY_INTERVAL_MS;
}

void versucheBmeRecovery(unsigned long jetzt) {
    if (bme280Bereit || !sensorRecoveryFaellig(letzterBmeRecoveryMs, jetzt)) return;

    letzterBmeRecoveryMs = jetzt;
    bme280Bereit = initialisiereBme280();
    logf(bme280Bereit ? "INFO" : "WARN",
         bme280Bereit ? "BME280 Recovery erfolgreich" : "BME280 Recovery fehlgeschlagen");
}

void versucheVemlRecovery(unsigned long jetzt) {
    if (veml7700Bereit || !sensorRecoveryFaellig(letzterVemlRecoveryMs, jetzt)) return;

    letzterVemlRecoveryMs = jetzt;
    veml7700Bereit = initialisiereVeml7700(jetzt);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 Recovery erfolgreich" : "VEML7700 Recovery fehlgeschlagen");
}

bool leseRegenNass() {
    const int pegel = digitalRead(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN);
#if NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW
    return pegel == LOW;
#else
    return pegel == HIGH;
#endif
}
}  // namespace

void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterBmeFehlerLogMs = 0UL;
    letzterVemlFehlerLogMs = 0UL;
    letzterSnapshotLogMs = 0UL;
    letzterBmeRecoveryMs = 0UL;
    letzterVemlRecoveryMs = 0UL;
    veml7700BereitSeitMs = 0UL;
    regenEventOffen = false;
    regenEventStatus = 0U;

    erweiterterState = {
        NET_SEN_PRESSURE_UNGUELTIG,
        NET_SEN_GAS_OHM_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG};
    erweiterterStateGeaendert = true;

    Wire.setClock(I2C_CLOCK_HZ);

    bme280Bereit = initialisiereBme280();
    if (!bme280Bereit) {
        logf(
            "WARN",
            "BME280 nicht gefunden (0x%02X/0x%02X)",
            NET_SEN_ENV_BME280_PRIMARY_ADDRESS,
            NET_SEN_ENV_BME280_FALLBACK_ADDRESS);
    }

    veml7700Bereit = initialisiereVeml7700(bootMs);
    if (veml7700Bereit) {
        logf("INFO", "VEML7700 init OK auf 0x10");
    } else {
        logf("WARN", "VEML7700 nicht gefunden (0x10)");
    }

#if NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT_PULLUP);
#else
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT);
#endif
    regenNass = leseRegenNass();
    logf(
        "INFO",
        "Regensensor init: pin=%d status=%s",
        NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN,
        regenNass ? "nass" : "trocken");
}

void netSenDeviceExtendedStateInit() {}

bool netSenDeviceExtendedStatePoll(
    uint32_t* pressure_pa,
    uint32_t* gas_ohm,
    uint16_t* aqi,
    uint16_t* tvoc_ppb,
    uint16_t* eco2_ppm)
{
    if (!pressure_pa || !gas_ohm || !aqi || !tvoc_ppb || !eco2_ppm) return false;

    *pressure_pa = erweiterterState.pressure_pa;
    *gas_ohm = erweiterterState.gas_ohm;
    *aqi = erweiterterState.aqi;
    *tvoc_ppb = erweiterterState.tvoc_ppb;
    *eco2_ppm = erweiterterState.eco2_ppm;

    const bool changed = erweiterterStateGeaendert;
    erweiterterStateGeaendert = false;
    return changed;
}

bool netSenDevicePollEvent(
    uint8_t* event_type,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2)
{
    if (!regenEventOffen) return false;
    regenEventOffen = false;

    if (event_type != nullptr) *event_type = SH_EVENT_RAIN_DETECTED;
    if (trigger != nullptr) *trigger = SH_TRIGGER_AUTO;
    if (param1 != nullptr) *param1 = regenEventStatus;
    if (param2 != nullptr) *param2 = 0U;
    return true;
}

bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault)
{
    if (!temp_01c || !hum_01pct || !lux || !motion || !fault) return false;

    const unsigned long jetzt = millis();
    if ((jetzt - letzterSensorPollMs) < NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS) {
        return false;
    }
    letzterSensorPollMs = jetzt;

    const int16_t vorherTemp = *temp_01c;
    const uint16_t vorherHum = *hum_01pct;
    const uint16_t vorherLux = *lux;
    const uint8_t vorherMotion = *motion;
    const bool vorherFault = *fault;

    int16_t neuerTemp = vorherTemp;
    uint16_t neuerHum = vorherHum;
    uint16_t neuerLux = vorherLux;
    const uint8_t neueMotion = 0U;
    uint32_t neuerPressure = NET_SEN_PRESSURE_UNGUELTIG;
    bool bmeMessungGueltig = false;
    bool vemlMessungGueltig = false;

    versucheBmeRecovery(jetzt);
    versucheVemlRecovery(jetzt);

    if (bme280Bereit) {
        const float tempC = sensorBme280.readTemperature();
        const float humPct = sensorBme280.readHumidity();
        const float pressurePa = sensorBme280.readPressure();

        const bool bmeGueltig =
            isfinite(tempC) &&
            isfinite(humPct) &&
            isfinite(pressurePa) &&
            humPct >= 0.0f &&
            humPct <= 100.0f &&
            pressurePa >= 30000.0f &&
            pressurePa <= 110000.0f;

        if (bmeGueltig) {
            neuerTemp = (int16_t)lroundf(tempC * 10.0f);
            neuerHum = clampHum01pct((long)lroundf(humPct * 10.0f));
            neuerPressure = (uint32_t)lroundf(pressurePa);
            bmeMessungGueltig = true;
        } else {
            bme280Bereit = false;
            logBmeFehlerGedrosselt(jetzt, "Messwerte unplausibel");
        }
    } else {
        logBmeFehlerGedrosselt(jetzt, "Sensor nicht initialisiert");
    }

    const bool vemlAufwaermphase =
        veml7700Bereit &&
        (jetzt - veml7700BereitSeitMs) < NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS;
    if (veml7700Bereit) {
        if (!vemlAufwaermphase) {
            const float luxWert = sensorVeml7700.readLux();
            if (isfinite(luxWert) && luxWert >= 0.0f) {
                neuerLux = clampToU16((long)lroundf(luxWert));
                vemlMessungGueltig = true;
            } else {
                veml7700Bereit = false;
                logVemlFehlerGedrosselt(jetzt, "Lux unplausibel");
            }
        }
    } else {
        logVemlFehlerGedrosselt(jetzt, "Sensor nicht initialisiert");
    }

    const bool neuerRegenNass = leseRegenNass();
    if (neuerRegenNass != regenNass) {
        regenNass = neuerRegenNass;
        regenEventStatus = regenNass ? 1U : 0U;
        regenEventOffen = true;
        logf("INFO", "Regensensor Status geaendert: %s", regenNass ? "nass" : "trocken");
    }

    const bool extendedGeaendert = uebernehmeWertU32(
        &erweiterterState.pressure_pa,
        neuerPressure,
        NET_SEN_PRESSURE_UNGUELTIG,
        NET_SEN_ENV_BME280_VEML_RAIN_PRESSURE_DELTA_PA);
    if (extendedGeaendert) {
        erweiterterStateGeaendert = true;
    }

    const bool neuerFault = !bmeMessungGueltig || (!vemlMessungGueltig && !vemlAufwaermphase);

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    if ((jetzt - letzterSnapshotLogMs) >= NET_SEN_ENV_BME280_VEML_RAIN_SNAPSHOT_LOG_INTERVAL_MS) {
        logf(
            "INFO",
            "ENV snapshot temp_01c=%d hum_01pct=%u lux=%u pressure_pa=%lu rain=%s fault=%s",
            neuerTemp,
            neuerHum,
            neuerLux,
            (unsigned long)erweiterterState.pressure_pa,
            regenNass ? "nass" : "trocken",
            neuerFault ? "true" : "false");
        letzterSnapshotLogMs = jetzt;
    }

    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_ENV_BME280_VEML_RAIN_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_ENV_BME280_VEML_RAIN_HUM_DELTA_01PCT ||
           absDiffU16(neuerLux, vorherLux) >= NET_SEN_ENV_BME280_VEML_RAIN_LUX_DELTA ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}
