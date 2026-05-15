/**
 * @file main.cpp
 * @brief NET-SEN DHT22 Reference: Temperatur/Feuchte-Sensor (Referenz-Implementierung)
 *
 * @details Custom-Sensor-Hook fuer DHT22 an GPIO6 (OneWire, Pullup).
 *          Messung alle 2500ms nach 2500ms Warmup-Phase.
 *          Werte in Zehntel (temp_01c, hum_01pct) mit Plausibilitaetspruefung.
 *          Hysterese: 1.0 Grad Temp, 5.0% Feuchte – sonst kein STATE-Update.
 *          Gedrosseltes Logging: Werte alle 15s, Fehler alle 30s.
 *
 * Hardware: ESP32-C3 + DHT22 an GPIO6
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 */

#include <Arduino.h>
#include <DHT.h>
#include <math.h>

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
using SmartHome::absDiffU16;
using SmartHome::absDiffI16;

#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault);

#include "../../basetypes/net_sen/NetSenRuntime.h"

// =============================================================================
// LOKALER ZUSTAND – DHT22-Sensor, Timer, Delta-Logging
// =============================================================================

namespace {
constexpr uint8_t DHT_SENSOR_TYPE = DHT22;
constexpr uint16_t HUM_01PCT_MIN = 0U;
constexpr uint16_t HUM_01PCT_MAX = 1000U;

DHT sensorDht22(NET_SEN_DHT22_REF_PIN_DATA, DHT_SENSOR_TYPE);
unsigned long bootMs = 0UL;                  // Boot-Zeitstempel (fuer Warmup)
unsigned long letzterSensorPollMs = 0UL;      // Letzte Messung
unsigned long letzterFehlerLogMs = 0UL;       // Gedrosseltes Fehler-Logging
unsigned long letzterWerteLogMs = 0UL;        // Gedrosseltes Werte-Logging
bool letzterFaultState = true;                // Vorheriger Fehlerstatus

/**
 * @brief Begrenzt Feuchte-Wert auf den gueltigen Bereich 0-1000 (0-100%).
 * @param value Rohwert in Zehntel-Prozent
 * @return Begrenzter Wert zwischen HUM_01PCT_MIN und HUM_01PCT_MAX
 */
uint16_t clampToHum01pct(long value) {
    if (value < (long)HUM_01PCT_MIN) return HUM_01PCT_MIN;
    if (value > (long)HUM_01PCT_MAX) return HUM_01PCT_MAX;
    return (uint16_t)value;
}
}  // namespace

// =============================================================================
// CUSTOM-SENSOR-HOOKS
// =============================================================================

/**
 * @brief Initialisiert den DHT22-Sensor und setzt Timer zurueck.
 *
 * Erfasst bootMs fuer die Warmup-Phase (NET_SEN_DHT22_REF_WARMUP_MS).
 * Wird einmalig von NetSenRuntime beim Boot aufgerufen.
 */
void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterFehlerLogMs = 0UL;
    letzterWerteLogMs = 0UL;
    letzterFaultState = true;

    sensorDht22.begin();
    logf("INFO", "DHT22 init (data_pin=%d, warmup_ms=%lu, read_interval_ms=%lu)",
         NET_SEN_DHT22_REF_PIN_DATA,
         NET_SEN_DHT22_REF_WARMUP_MS,
         NET_SEN_DHT22_REF_READ_INTERVAL_MS);
}

/**
 * @brief DHT22-Messung mit Plausibilitaetspruefung und Hysterese.
 *
 * Misst Temperatur und Feuchte vom DHT22 (nach Warmup-Phase).
 * Werte werden auf Bereich (TEMP_MIN..MAX, HUM_MIN..MAX) und NaN geprueft.
 * Gedrosseltes Logging: Werte nur alle VALUE_LOG_INTERVAL_MS,
 * Fehler nur alle ERROR_LOG_INTERVAL_MS.
 *
 * @param[in,out] temp_01c  Temperatur in Zehntel-Grad Celsius
 * @param[in,out] hum_01pct Feuchte in Zehntel-Prozent
 * @param[out]    lux       immer 0 (kein Lux-Sensor)
 * @param[out]    motion    immer 0 (kein PIR-Sensor)
 * @param[out]    fault     true bei fehlerhafter/ungueltiger Messung
 * @return true wenn sich mindestens ein Wert signifikant geaendert hat
 */
bool netSenDeviceSensorPoll(
    int16_t* temp_01c, uint16_t* hum_01pct,
    uint16_t* lux, uint8_t* motion, bool* fault)
{
    if (!temp_01c || !hum_01pct || !lux || !motion || !fault) return false;

    const unsigned long jetzt = millis();
    // Prueft ob Read-Intervall (2500ms) abgelaufen
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

    // Prueft ob Warmup-Phase (2500ms) abgelaufen
    if ((jetzt - bootMs) >= NET_SEN_DHT22_REF_WARMUP_MS) {
        const float tempC = sensorDht22.readTemperature();
        const float humPct = sensorDht22.readHumidity();
        const bool rohwerteGueltig = isfinite(tempC) && isfinite(humPct);
        const int16_t temp01c = (int16_t)lroundf(tempC * 10.0f);
        const uint16_t hum01pct = clampToHum01pct((long)lroundf(humPct * 10.0f));

        // Plausibilitaetspruefung: Bereich + NaN-Check
        const bool messungGueltig =
            rohwerteGueltig &&
            temp01c >= NET_SEN_DHT22_REF_TEMP_MIN_01C &&
            temp01c <= NET_SEN_DHT22_REF_TEMP_MAX_01C &&
            hum01pct >= NET_SEN_DHT22_REF_HUM_MIN_01PCT &&
            hum01pct <= NET_SEN_DHT22_REF_HUM_MAX_01PCT;

        if (messungGueltig) {
            neuerTemp = temp01c;
            neuerHum = hum01pct;
            neuerFault = false;

            // Gedrosseltes Logging: nur alle 15s
            if ((jetzt - letzterWerteLogMs) >= NET_SEN_DHT22_REF_VALUE_LOG_INTERVAL_MS
                || vorherFault) {
                logf("INFO", "DHT22 Messwert temp_01c=%d hum_01pct=%u",
                     (int)neuerTemp, (unsigned int)neuerHum);
                letzterWerteLogMs = jetzt;
            }
        } else if ((jetzt - letzterFehlerLogMs) >= NET_SEN_DHT22_REF_ERROR_LOG_INTERVAL_MS
                   || !vorherFault) {
            logf("WARN", "DHT22 Messung ungueltig (tempC=%.2f humPct=%.2f ...)",
                 tempC, humPct);
            letzterFehlerLogMs = jetzt;
        }
    }

    // Log bei Uebergang von Fehler zu Erfolg
    if (letzterFaultState && !neuerFault) {
        logf("INFO", "DHT22 liefert gueltige Messwerte");
    }
    letzterFaultState = neuerFault;

    *temp_01c = neuerTemp;
    *hum_01pct = neuerHum;
    *lux = neuerLux;
    *motion = neueMotion;
    *fault = neuerFault;

    // Rueckgabe: true bei signifikanter Aenderung (Hysterese)
    return absDiffI16(neuerTemp, vorherTemp) >= NET_SEN_DHT22_REF_TEMP_DELTA_01C ||
           absDiffU16(neuerHum, vorherHum) >= NET_SEN_DHT22_REF_HUM_DELTA_01PCT ||
           neuerLux != vorherLux ||
           neueMotion != vorherMotion ||
           neuerFault != vorherFault;
}
