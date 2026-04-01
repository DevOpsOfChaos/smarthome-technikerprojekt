#include <Arduino.h>
#include <Wire.h>
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

#if NET_SEN_ROOM_USE_BME280
  #include <Adafruit_BME280.h>
#endif

#if NET_SEN_ROOM_USE_VEML7700
  #include <Adafruit_VEML7700.h>
#endif

static_assert(
    NET_SEN_ROOM_USE_BME280 || NET_SEN_ROOM_USE_VEML7700,
    "net_sen_room braucht mindestens einen aktiven Sensorpfad.");

namespace {
#if NET_SEN_ROOM_USE_BME280
Adafruit_BME280 sensorBme280;
bool bme280Bereit = false;
#endif

#if NET_SEN_ROOM_USE_VEML7700
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();
bool veml7700Bereit = false;
#endif

unsigned long letzterSensorPollMs = 0UL;

uint16_t clampToU16(long value) {
    if (value < 0L) return 0U;
    if (value > 65535L) return 65535U;
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
#if NET_SEN_ROOM_USE_BME280
    bme280Bereit = sensorBme280.begin((uint8_t)NET_SEN_ROOM_BME280_ADDRESS, &Wire);
    if (!bme280Bereit) {
        logf("WARN", "BME280 nicht gefunden (addr=0x%02X)", NET_SEN_ROOM_BME280_ADDRESS);
    }
#endif

#if NET_SEN_ROOM_USE_VEML7700
    veml7700Bereit = sensorVeml7700.begin();
    if (!veml7700Bereit) {
        logf("WARN", "VEML7700 nicht gefunden");
    }
#endif

    letzterSensorPollMs = 0UL;
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
    if ((jetzt - letzterSensorPollMs) < NET_SEN_ROOM_SENSOR_READ_INTERVAL_MS) {
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
    bool neuerFault = false;

#if NET_SEN_ROOM_USE_BME280
    if (!bme280Bereit) {
        neuerFault = true;
    } else {
        const float temp = sensorBme280.readTemperature();
        const float hum = sensorBme280.readHumidity();
        if (!isfinite(temp) || !isfinite(hum)) {
            neuerFault = true;
        } else {
            neuerTemp = (int16_t)lroundf(temp * 10.0f);
            neuerHum = clampToU16((long)lroundf(hum * 10.0f));
        }
    }
#endif

#if NET_SEN_ROOM_USE_VEML7700
    if (!veml7700Bereit) {
        neuerFault = true;
    } else {
        const float luxWert = sensorVeml7700.readLux();
        if (!isfinite(luxWert) || luxWert < 0.0f) {
            neuerFault = true;
        } else {
            neuerLux = clampToU16((long)lroundf(luxWert));
        }
    }
#endif

    // net_sen_room hat keine Bewegungssensorik.
    const uint8_t neueMotion = 0U;

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_ROOM_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_ROOM_HUM_DELTA_01PCT ||
           absDiffU16(neuerLux, vorherLux) >= NET_SEN_ROOM_LUX_DELTA ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}
