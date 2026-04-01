#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault);

#include "../../basetypes/net_sen/main.cpp"

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;

Adafruit_BME680 sensorBme680;
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();

bool bme680Bereit = false;
bool veml7700Bereit = false;
uint8_t bme680Adresse = 0U;

unsigned long bootMs = 0UL;
unsigned long letzterSensorPollMs = 0UL;
unsigned long letzterBmeFehlerLogMs = 0UL;
unsigned long letzterVemlFehlerLogMs = 0UL;

uint8_t bme680GueltigeMessungen = 0U;
bool gasWarmupInfoGeloggt = false;

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
}  // namespace

void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterBmeFehlerLogMs = 0UL;
    letzterVemlFehlerLogMs = 0UL;
    bme680GueltigeMessungen = 0U;
    gasWarmupInfoGeloggt = false;

    Wire.setClock(I2C_CLOCK_HZ);

    bme680Bereit = initialisiereBme680();
    if (!bme680Bereit) {
        logf("WARN", "BME680 nicht gefunden (0x%02X/0x%02X)", NET_SEN_ENV_BME680_PRIMARY_ADDRESS, NET_SEN_ENV_BME680_FALLBACK_ADDRESS);
    }

    veml7700Bereit = sensorVeml7700.begin();
    if (veml7700Bereit) {
        sensorVeml7700.setGain(VEML7700_GAIN_1);
        sensorVeml7700.setIntegrationTime(VEML7700_IT_400MS);
        logf("INFO", "VEML7700 init OK auf 0x10 (gain=1x it=400ms)");
    } else {
        logf("WARN", "VEML7700 nicht gefunden (0x10)");
    }
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

    bool bmeMessungGueltig = false;
    bool vemlMessungGueltig = false;

    if (bme680Bereit) {
        if (sensorBme680.performReading()) {
            const float tempC = sensorBme680.temperature;
            const float humPct = sensorBme680.humidity;
            const float pressurePa = sensorBme680.pressure;
            const uint32_t gasOhm = (uint32_t)sensorBme680.gas_resistance;

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
                bmeMessungGueltig = true;
                if (bme680GueltigeMessungen < 255U) bme680GueltigeMessungen++;

                if (istGasWarmupAbgeschlossen(jetzt) &&
                    !gasWarmupInfoGeloggt &&
                    gasOhm > 0UL) {
                    logf("INFO", "BME680 Rohgas warmup abgeschlossen (gas_ohm intern belastbar)");
                    gasWarmupInfoGeloggt = true;
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

    const bool neuerFault = !(bmeMessungGueltig && vemlMessungGueltig);

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_ENV_BME680_VEML_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_ENV_BME680_VEML_HUM_DELTA_01PCT ||
           absDiffU16(neuerLux, vorherLux) >= NET_SEN_ENV_BME680_VEML_LUX_DELTA ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}
