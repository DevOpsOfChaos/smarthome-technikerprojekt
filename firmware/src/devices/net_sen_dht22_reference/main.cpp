// =============================================================================
// main.cpp – NET-SEN DHT22 Reference: Temperatur/Feuchte-Sensor
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_dht22_reference/main.cpp
// Hardware:   ESP32-C3 + DHT22 an GPIO6
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   DHT22-Daten: GPIO6 (OneWire, Pullup erforderlich)
//
// Funktionsweise:
//   Custom-Sensor-Hook fr DHT22. Messung alle 2500ms nach 2500ms Warmup.
//   Werte in Zehntel (temp_01c, hum_01pct) mit Plausibilitaetspruefung.
//   Hysterese: 1.0 Grad Temp, 5.0% Feuchte – sonst kein STATE-Update.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

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

// clampToHum01pct – Begrenzt Feuchte auf 0-1000 (0-100%)
uint16_t clampToHum01pct(long value) {
    if (value < (long)HUM_01PCT_MIN) return HUM_01PCT_MIN;
    if (value > (long)HUM_01PCT_MAX) return HUM_01PCT_MAX;
    return (uint16_t)value;
}

// absDiffU16 – Absolute Differenz zweier uint16-Werte
uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

int16_t absDiffI16(int16_t a, int16_t b) {
    return a > b ? (int16_t)(a - b) : (int16_t)(b - a);
}
}  // namespace

// =============================================================================
// CUSTOM-SENSOR-HOOKS
// =============================================================================

// netSenDeviceSensorInit – Initialisiert DHT22 und Timer
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

// netSenDeviceSensorPoll – DHT22-Messung mit Plausibilitaet und Hysterese
//   Parameter: temp_01c – Ausgabe: Temperatur in Zehntel-Grad
//              hum_01pct – Ausgabe: Feuchte in Zehntel-Prozent
//              lux       – Ausgabe: immer 0 (kein Lux-Sensor)
//              motion    – Ausgabe: immer 0 (kein PIR)
//              fault     – Ausgabe: true bei fehlerhafter Messung
//   Rückgabe: true = Werte haben sich signifikant geaendert
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
