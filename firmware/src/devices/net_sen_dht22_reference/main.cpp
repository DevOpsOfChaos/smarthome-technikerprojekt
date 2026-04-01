#include <Arduino.h>
#include <DHT.h>
#include <math.h>

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
constexpr uint8_t DHT_SENSOR_TYPE = DHT22;
constexpr uint16_t HUM_01PCT_MIN = 0U;
constexpr uint16_t HUM_01PCT_MAX = 1000U;

DHT sensorDht22(NET_SEN_DHT22_REF_PIN_DATA, DHT_SENSOR_TYPE);
unsigned long bootMs = 0UL;
unsigned long letzterSensorPollMs = 0UL;
unsigned long letzterFehlerLogMs = 0UL;
bool letzterFaultState = true;

uint16_t clampToHum01pct(long value) {
    if (value < (long)HUM_01PCT_MIN) return HUM_01PCT_MIN;
    if (value > (long)HUM_01PCT_MAX) return HUM_01PCT_MAX;
    return (uint16_t)value;
}

uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

int16_t absDiffI16(int16_t a, int16_t b) {
    return a > b ? (int16_t)(a - b) : (int16_t)(b - a);
}
}  // namespace

void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterFehlerLogMs = 0UL;
    letzterFaultState = true;

    sensorDht22.begin();
    logf(
        "INFO",
        "DHT22 Referenzpfad initialisiert (data_pin=%d, warmup_ms=%lu, read_interval_ms=%lu)",
        NET_SEN_DHT22_REF_PIN_DATA,
        NET_SEN_DHT22_REF_WARMUP_MS,
        NET_SEN_DHT22_REF_READ_INTERVAL_MS);
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
    if ((jetzt - letzterSensorPollMs) < NET_SEN_DHT22_REF_READ_INTERVAL_MS) {
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
    uint16_t neuerLux = 0U;
    const uint8_t neueMotion = 0U;
    bool neuerFault = true;

    if ((jetzt - bootMs) >= NET_SEN_DHT22_REF_WARMUP_MS) {
        const float tempC = sensorDht22.readTemperature();
        const float humPct = sensorDht22.readHumidity();

        const bool messungGueltig =
            isfinite(tempC) &&
            isfinite(humPct) &&
            humPct >= 0.0f &&
            humPct <= 100.0f;

        if (messungGueltig) {
            neuerTemp = (int16_t)lroundf(tempC * 10.0f);
            neuerHum = clampToHum01pct((long)lroundf(humPct * 10.0f));
            neuerFault = false;
        } else if ((jetzt - letzterFehlerLogMs) >= NET_SEN_DHT22_REF_ERROR_LOG_INTERVAL_MS || !vorherFault) {
            logf("WARN", "DHT22 Messung ungueltig (temp/hum nicht plausibel)");
            letzterFehlerLogMs = jetzt;
        }
    }

    if (letzterFaultState && !neuerFault) {
        logf("INFO", "DHT22 liefert gueltige Messwerte");
    }
    letzterFaultState = neuerFault;

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_DHT22_REF_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_DHT22_REF_HUM_DELTA_01PCT ||
           neuerLux != vorherLux ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}
