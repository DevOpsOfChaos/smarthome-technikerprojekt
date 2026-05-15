// =============================================================================
// main.cpp – NET-SEN Room (Legacy): Temperatur, Feuchte, Lux
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_room/main.cpp
// Hardware:   ESP32-C3 + BME280 + VEML7700
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA:  GPIO0
//   I2C SCL:  GPIO1
//   BME280:   Adresse 0x76 (Temperatur + Feuchte)
//   VEML7700: Adresse 0x10 (Lux)
//
// Funktionsweise:
//   Custom-Sensor-Hook mit #if-optionalem BME280 und VEML7700.
//   Messung alle 2500ms. Werte in Zehntel (temp_01c, hum_01pct).
//   Keine Bewegungssensorik, kein Druck, kein Gas.
//
// Status: LEGACY – nicht Teil der aktiven net_sen-Linie.
// Offizielle Linie: net_sen_dht22_reference, net_sen_env_bme680_veml
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
using SmartHome::clampToU16;
using SmartHome::absDiffU16;
using SmartHome::absDiffI16;

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(
    int16_t* temp_01c, uint16_t* hum_01pct,
    uint16_t* lux, uint8_t* motion, bool* fault);

#include "../../basetypes/net_sen/NetSenRuntime.h"

#if NET_SEN_ROOM_USE_BME280
  #include <Adafruit_BME280.h>
#endif

#if NET_SEN_ROOM_USE_VEML7700
  #include <Adafruit_VEML7700.h>
#endif

// Prueft ob mindestens ein Sensor aktiviert ist
static_assert(NET_SEN_ROOM_USE_BME280 || NET_SEN_ROOM_USE_VEML7700,
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
}  // namespace

void netSenDeviceSensorInit() {
#if NET_SEN_ROOM_USE_BME280
    bme280Bereit = sensorBme280.begin((uint8_t)NET_SEN_ROOM_BME280_ADDRESS, &Wire);
    logf(bme280Bereit ? "INFO" : "WARN",
         bme280Bereit ? "BME280 init OK" : "BME280 nicht gefunden (addr=0x%02X)",
         NET_SEN_ROOM_BME280_ADDRESS);
#endif

#if NET_SEN_ROOM_USE_VEML7700
    veml7700Bereit = sensorVeml7700.begin();
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 init OK" : "VEML7700 nicht gefunden");
#endif

    letzterSensorPollMs = 0UL;
}

bool netSenDeviceSensorPoll(
    int16_t* temp_01c, uint16_t* hum_01pct,
    uint16_t* lux, uint8_t* motion, bool* fault)
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
