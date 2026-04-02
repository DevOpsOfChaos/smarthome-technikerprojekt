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

#include "../../basetypes/net_sen/NetSenRuntime.h"

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
constexpr uint32_t SENSOR_SNAPSHOT_LOG_INTERVAL_MS = 30000UL;
constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;

struct ErweiterterState {
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
};

Adafruit_BME680 sensorBme680;
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();
ScioSense_ENS160 sensorEns160Addr52(NET_SEN_ENV_ENS160_PRIMARY_ADDRESS);
ScioSense_ENS160 sensorEns160Addr53(NET_SEN_ENV_ENS160_FALLBACK_ADDRESS);
ScioSense_ENS160* sensorEns160 = nullptr;

bool bme680Bereit = false;
bool veml7700Bereit = false;
bool ens160Bereit = false;
uint8_t bme680Adresse = 0U;
uint8_t ens160Adresse = 0U;

unsigned long bootMs = 0UL;
unsigned long letzterSensorPollMs = 0UL;
unsigned long letzterBmeFehlerLogMs = 0UL;
unsigned long letzterVemlFehlerLogMs = 0UL;
unsigned long letzterEnsInfoLogMs = 0UL;
unsigned long letzterEnsFehlerLogMs = 0UL;
unsigned long letzterEnsGueltigMs = 0UL;
unsigned long letzterSnapshotLogMs = 0UL;

uint8_t bme680GueltigeMessungen = 0U;
bool gasWarmupInfoGeloggt = false;
bool ensWarteHinweisGeloggt = false;

bool ensCompensationAktiv = false;
bool ensCompCall = false;
int ensCompResult = -1;

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

int16_t absDiffI16(int16_t a, int16_t b) {
    return a > b ? (int16_t)(a - b) : (int16_t)(b - a);
}

bool istGasWarmupAbgeschlossen(unsigned long jetztMs) {
    return (jetztMs - bootMs) >= NET_SEN_ENV_BME680_GAS_WARMUP_MS &&
           bme680GueltigeMessungen >= NET_SEN_ENV_BME680_GAS_WARMUP_MIN_READS;
}

bool istEns160WarmupAbgeschlossen(unsigned long jetztMs) {
    return (jetztMs - bootMs) >= NET_SEN_ENV_ENS160_WARMUP_MS;
}

bool ens160FehltZuLange(unsigned long jetztMs) {
    if (!istEns160WarmupAbgeschlossen(jetztMs)) return false;
    if (!ens160Bereit) return true;
    if (letzterEnsGueltigMs == 0UL) return true;
    return (jetztMs - letzterEnsGueltigMs) > NET_SEN_ENV_ENS160_STALE_TIMEOUT_MS;
}

bool wertAendertU32(uint32_t alt, uint32_t neu, uint32_t invalid, uint32_t delta) {
    if (alt == neu) return false;
    if (alt == invalid || neu == invalid) return true;
    const uint32_t diff = alt > neu ? (alt - neu) : (neu - alt);
    return diff >= delta;
}

bool wertAendertU16(uint16_t alt, uint16_t neu, uint16_t invalid, uint16_t delta) {
    if (alt == neu) return false;
    if (alt == invalid || neu == invalid) return true;
    const uint16_t diff = alt > neu ? (uint16_t)(alt - neu) : (uint16_t)(neu - alt);
    return diff >= delta;
}

uint16_t mapEns160AqiZu500(uint16_t rawAqi) {
    if (rawAqi >= 1U && rawAqi <= ENS160_AQI_MAX_BASIC) {
        return (uint16_t)(rawAqi * 100U);
    }
    return 0U;
}

uint16_t encodeEns160Temp(float temperaturC) {
    return (uint16_t)((temperaturC + 273.15f) * 64.0f);
}

uint16_t encodeEns160Humidity(float feuchtePct) {
    return (uint16_t)(feuchtePct * 512.0f);
}

int writeEns160EnvData(uint8_t address, float temperaturC, float feuchtePct) {
    uint8_t payload[4];
    const uint16_t tempEncoded = encodeEns160Temp(temperaturC);
    const uint16_t humidityEncoded = encodeEns160Humidity(feuchtePct);

    payload[0] = (uint8_t)(tempEncoded & 0xFFU);
    payload[1] = (uint8_t)((tempEncoded >> 8) & 0xFFU);
    payload[2] = (uint8_t)(humidityEncoded & 0xFFU);
    payload[3] = (uint8_t)((humidityEncoded >> 8) & 0xFFU);

    Wire.beginTransmission(address);
    Wire.write(ENS160_REG_TEMP_IN);
    Wire.write(payload, sizeof(payload));
    return (int)Wire.endTransmission();
}

void logBmeFehlerGedrosselt(unsigned long jetztMs, const char* grund) {
    if ((jetztMs - letzterBmeFehlerLogMs) < NET_SEN_ENV_BME680_VEML_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "BME680 FEHLER: %s", grund);
    letzterBmeFehlerLogMs = jetztMs;
}

void logVemlFehlerGedrosselt(unsigned long jetztMs, const char* grund) {
    if ((jetztMs - letzterVemlFehlerLogMs) < NET_SEN_ENV_BME680_VEML_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "VEML7700 FEHLER: %s", grund);
    letzterVemlFehlerLogMs = jetztMs;
}

void logEnsInfoGedrosselt(unsigned long jetztMs, const char* status, bool bmeValid) {
    if ((jetztMs - letzterEnsInfoLogMs) < NET_SEN_ENV_BME680_VEML_ERROR_LOG_INTERVAL_MS) return;
    logf(
        "INFO",
        "ENS160 status=%s kompensation=%s bme_valid=%s ens_comp_call=%s ens_comp_result=%d",
        status,
        ensCompensationAktiv ? "aktiv" : "aus",
        bmeValid ? "true" : "false",
        ensCompCall ? "ja" : "nein",
        ensCompResult);
    letzterEnsInfoLogMs = jetztMs;
}

void logEnsFehlerGedrosselt(unsigned long jetztMs, const char* grund, bool bmeValid) {
    if ((jetztMs - letzterEnsFehlerLogMs) < NET_SEN_ENV_BME680_VEML_ERROR_LOG_INTERVAL_MS) return;
    logf(
        "WARN",
        "ENS160 FEHLER: %s (kompensation=%s bme_valid=%s ens_comp_call=%s ens_comp_result=%d)",
        grund,
        ensCompensationAktiv ? "aktiv" : "aus",
        bmeValid ? "true" : "false",
        ensCompCall ? "ja" : "nein",
        ensCompResult);
    letzterEnsFehlerLogMs = jetztMs;
}

bool initialisiereBme680() {
    const uint8_t moeglicheAdressen[] = {
        (uint8_t)NET_SEN_ENV_BME680_PRIMARY_ADDRESS,
        (uint8_t)NET_SEN_ENV_BME680_FALLBACK_ADDRESS};

    for (uint8_t adresse : moeglicheAdressen) {
        if (!sensorBme680.begin(adresse, &Wire)) continue;

        sensorBme680.setTemperatureOversampling(BME680_OS_8X);
        sensorBme680.setHumidityOversampling(BME680_OS_2X);
        sensorBme680.setPressureOversampling(BME680_OS_4X);
        sensorBme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
        sensorBme680.setGasHeater(320U, 150U);

        bme680Adresse = adresse;
        logf("INFO", "BME680 init OK auf 0x%02X", bme680Adresse);
        return true;
    }

    return false;
}

bool initialisiereEns160() {
    sensorEns160 = &sensorEns160Addr52;
    if (sensorEns160->begin()) {
        ens160Adresse = NET_SEN_ENV_ENS160_PRIMARY_ADDRESS;
    } else {
        sensorEns160 = &sensorEns160Addr53;
        if (!sensorEns160->begin()) {
            sensorEns160 = nullptr;
            ens160Adresse = 0U;
            return false;
        }
        ens160Adresse = NET_SEN_ENV_ENS160_FALLBACK_ADDRESS;
    }

    if (!sensorEns160->setMode(ENS160_OPMODE_STD)) {
        sensorEns160 = nullptr;
        ens160Adresse = 0U;
        return false;
    }

    logf("INFO", "ENS160 init OK auf 0x%02X", ens160Adresse);
    return true;
}

bool uebernehmeWertU32(uint32_t* ziel, uint32_t neu, uint32_t invalid, uint32_t delta) {
    if (!ziel) return false;
    const bool changed = wertAendertU32(*ziel, neu, invalid, delta);
    *ziel = neu;
    return changed;
}

bool uebernehmeWertU16(uint16_t* ziel, uint16_t neu, uint16_t invalid, uint16_t delta) {
    if (!ziel) return false;
    const bool changed = wertAendertU16(*ziel, neu, invalid, delta);
    *ziel = neu;
    return changed;
}

void aktualisiereEnsKompensation(bool bmeValid, float tempC, float humPct) {
    ensCompensationAktiv = false;
    ensCompCall = false;
    ensCompResult = -1;

    if (!ens160Bereit || !sensorEns160 || !bmeValid) return;

    ensCompCall = true;
    ensCompResult = writeEns160EnvData(ens160Adresse, tempC, humPct);
    ensCompensationAktiv = (ensCompResult == 0);
}
}  // namespace

void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterBmeFehlerLogMs = 0UL;
    letzterVemlFehlerLogMs = 0UL;
    letzterEnsInfoLogMs = 0UL;
    letzterEnsFehlerLogMs = 0UL;
    letzterEnsGueltigMs = 0UL;
    letzterSnapshotLogMs = 0UL;
    bme680GueltigeMessungen = 0U;
    gasWarmupInfoGeloggt = false;
    ensWarteHinweisGeloggt = false;
    ensCompensationAktiv = false;
    ensCompCall = false;
    ensCompResult = -1;

    erweiterterState = {
        NET_SEN_PRESSURE_UNGUELTIG,
        NET_SEN_GAS_OHM_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG,
        NET_SEN_AIR_METRIC_UNGUELTIG};
    erweiterterStateGeaendert = true;

    Wire.setClock(I2C_CLOCK_HZ);

    bme680Bereit = initialisiereBme680();
    if (!bme680Bereit) {
        logf(
            "WARN",
            "BME680 nicht gefunden (0x%02X/0x%02X)",
            NET_SEN_ENV_BME680_PRIMARY_ADDRESS,
            NET_SEN_ENV_BME680_FALLBACK_ADDRESS);
    }

    veml7700Bereit = sensorVeml7700.begin();
    if (veml7700Bereit) {
        sensorVeml7700.setGain(VEML7700_GAIN_1);
        sensorVeml7700.setIntegrationTime(VEML7700_IT_400MS);
        logf("INFO", "VEML7700 init OK auf 0x10 (gain=1x it=400ms)");
    } else {
        logf("WARN", "VEML7700 nicht gefunden (0x10)");
    }

    ens160Bereit = initialisiereEns160();
    if (!ens160Bereit) {
        logf(
            "WARN",
            "ENS160 nicht gefunden (0x%02X/0x%02X)",
            NET_SEN_ENV_ENS160_PRIMARY_ADDRESS,
            NET_SEN_ENV_ENS160_FALLBACK_ADDRESS);
    }
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

bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault)
{
    if (!temp_01c || !hum_01pct || !lux || !motion || !fault) return false;

    const unsigned long jetzt = millis();
    if ((jetzt - letzterSensorPollMs) < NET_SEN_ENV_BME680_VEML_SENSOR_READ_INTERVAL_MS) {
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

    float bmeTempC = NAN;
    float bmeHumPct = NAN;
    bool bmeMessungGueltig = false;
    bool vemlMessungGueltig = false;
    uint32_t neuerPressure = NET_SEN_PRESSURE_UNGUELTIG;
    uint32_t neuerGasOhm = NET_SEN_GAS_OHM_UNGUELTIG;

    if (bme680Bereit) {
        if (sensorBme680.performReading()) {
            bmeTempC = sensorBme680.temperature;
            bmeHumPct = sensorBme680.humidity;
            const float pressurePa = sensorBme680.pressure;
            const uint32_t gasOhm = (uint32_t)sensorBme680.gas_resistance;

            const bool bmeGueltig =
                isfinite(bmeTempC) &&
                isfinite(bmeHumPct) &&
                isfinite(pressurePa) &&
                bmeHumPct >= 0.0f &&
                bmeHumPct <= 100.0f &&
                pressurePa >= 30000.0f &&
                pressurePa <= 110000.0f;

            if (bmeGueltig) {
                neuerTemp = (int16_t)lroundf(bmeTempC * 10.0f);
                neuerHum = clampHum01pct((long)lroundf(bmeHumPct * 10.0f));
                neuerPressure = (uint32_t)lroundf(pressurePa);
                bmeMessungGueltig = true;
                if (bme680GueltigeMessungen < 255U) bme680GueltigeMessungen++;

                if (istGasWarmupAbgeschlossen(jetzt) && gasOhm > 0UL) {
                    neuerGasOhm = gasOhm;
                    if (!gasWarmupInfoGeloggt) {
                        logf("INFO", "BME680 Rohgas warmup abgeschlossen (gas_ohm intern belastbar)");
                        gasWarmupInfoGeloggt = true;
                    }
                }
            } else {
                logBmeFehlerGedrosselt(jetzt, "Messwerte unplausibel");
            }
        } else {
            logBmeFehlerGedrosselt(jetzt, "performReading fehlgeschlagen");
        }
    } else {
        logBmeFehlerGedrosselt(jetzt, "Sensor nicht initialisiert");
    }

    if (veml7700Bereit) {
        if ((jetzt - bootMs) >= NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS) {
            const float luxWert = sensorVeml7700.readLux();
            if (isfinite(luxWert) && luxWert >= 0.0f) {
                neuerLux = clampToU16((long)lroundf(luxWert));
                vemlMessungGueltig = true;
            } else {
                logVemlFehlerGedrosselt(jetzt, "Lux unplausibel");
            }
        }
    } else {
        logVemlFehlerGedrosselt(jetzt, "Sensor nicht initialisiert");
    }

    aktualisiereEnsKompensation(bmeMessungGueltig, bmeTempC, bmeHumPct);
    if (ensCompCall && !ensCompensationAktiv) {
        logEnsFehlerGedrosselt(jetzt, "ENV-Write fehlgeschlagen", bmeMessungGueltig);
    }

    uint16_t neuerAqi = erweiterterState.aqi;
    uint16_t neuerTvoc = erweiterterState.tvoc_ppb;
    uint16_t neuerEco2 = erweiterterState.eco2_ppm;
    bool ensMessungGueltig = false;

    if (ens160Bereit && sensorEns160 != nullptr) {
        const bool neueDaten = sensorEns160->measure(false);
        if (neueDaten) {
            const uint16_t rawAqi500 = sensorEns160->getAQI500();
            const uint16_t rawAqi = sensorEns160->getAQI();
            const uint16_t mappedAqi =
                (rawAqi500 > 0U && rawAqi500 <= 500U) ? rawAqi500 : mapEns160AqiZu500(rawAqi);
            const uint16_t tvoc = sensorEns160->getTVOC();
            const uint16_t eco2 = sensorEns160->geteCO2();

            if (mappedAqi > 0U) {
                neuerAqi = mappedAqi;
                neuerTvoc = tvoc;
                neuerEco2 = eco2;
                ensMessungGueltig = true;
                letzterEnsGueltigMs = jetzt;
                ensWarteHinweisGeloggt = false;
                logEnsInfoGedrosselt(
                    jetzt,
                    istEns160WarmupAbgeschlossen(jetzt) ? "brauchbar" : "warmup",
                    bmeMessungGueltig);
            } else {
                logEnsInfoGedrosselt(
                    jetzt,
                    istEns160WarmupAbgeschlossen(jetzt) ? "ungueltige_ausgabe" : "warmup",
                    bmeMessungGueltig);
            }
        } else if (!ensWarteHinweisGeloggt && !istEns160WarmupAbgeschlossen(jetzt)) {
            logEnsInfoGedrosselt(jetzt, "warte_auf_daten", bmeMessungGueltig);
            ensWarteHinweisGeloggt = true;
        }
    }

    if (!ensMessungGueltig && ens160FehltZuLange(jetzt)) {
        neuerAqi = NET_SEN_AIR_METRIC_UNGUELTIG;
        neuerTvoc = NET_SEN_AIR_METRIC_UNGUELTIG;
        neuerEco2 = NET_SEN_AIR_METRIC_UNGUELTIG;
        logEnsFehlerGedrosselt(jetzt, "zu lange keine gueltigen Daten", bmeMessungGueltig);
    }

    bool extendedGeaendert = false;
    extendedGeaendert =
        uebernehmeWertU32(
            &erweiterterState.pressure_pa,
            neuerPressure,
            NET_SEN_PRESSURE_UNGUELTIG,
            NET_SEN_ENV_BME680_VEML_PRESSURE_DELTA_PA) || extendedGeaendert;
    extendedGeaendert =
        uebernehmeWertU32(
            &erweiterterState.gas_ohm,
            neuerGasOhm,
            NET_SEN_GAS_OHM_UNGUELTIG,
            NET_SEN_ENV_BME680_VEML_GAS_DELTA_OHM) || extendedGeaendert;
    extendedGeaendert =
        uebernehmeWertU16(
            &erweiterterState.aqi,
            neuerAqi,
            NET_SEN_AIR_METRIC_UNGUELTIG,
            NET_SEN_ENV_BME680_VEML_AQI_DELTA) || extendedGeaendert;
    extendedGeaendert =
        uebernehmeWertU16(
            &erweiterterState.tvoc_ppb,
            neuerTvoc,
            NET_SEN_AIR_METRIC_UNGUELTIG,
            NET_SEN_ENV_BME680_VEML_TVOC_DELTA_PPB) || extendedGeaendert;
    extendedGeaendert =
        uebernehmeWertU16(
            &erweiterterState.eco2_ppm,
            neuerEco2,
            NET_SEN_AIR_METRIC_UNGUELTIG,
            NET_SEN_ENV_BME680_VEML_ECO2_DELTA_PPM) || extendedGeaendert;

    const bool ensFault = ens160FehltZuLange(jetzt);
    const bool neuerFault = !(bmeMessungGueltig && vemlMessungGueltig) || ensFault;

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    if (extendedGeaendert) {
        erweiterterStateGeaendert = true;
    }

    if ((jetzt - letzterSnapshotLogMs) >= SENSOR_SNAPSHOT_LOG_INTERVAL_MS) {
        logf(
            "INFO",
            "ENV snapshot temp_01c=%d hum_01pct=%u lux=%u pressure_pa=%lu gas_ohm=%lu aqi=%u tvoc_ppb=%u eco2_ppm=%u fault=%s",
            neuerTemp,
            neuerHum,
            neuerLux,
            (unsigned long)erweiterterState.pressure_pa,
            (unsigned long)erweiterterState.gas_ohm,
            erweiterterState.aqi,
            erweiterterState.tvoc_ppb,
            erweiterterState.eco2_ppm,
            neuerFault ? "true" : "false");
        letzterSnapshotLogMs = jetzt;
    }

    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_ENV_BME680_VEML_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_ENV_BME680_VEML_HUM_DELTA_01PCT ||
           absDiffU16(neuerLux, vorherLux) >= NET_SEN_ENV_BME680_VEML_LUX_DELTA ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}

