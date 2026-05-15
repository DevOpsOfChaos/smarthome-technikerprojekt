/**
 * @file main.cpp
 * @brief BAT-SEN Rain: Batteriebetriebener Regensensor
 *
 * @details Timer-Wake alle 15 Minuten, ADC-Messung mit Hysterese (2050/2200),
 *          Event bei Zustandswechsel (nass/trocken), Deep-Sleep dazwischen.
 *
 * Hardware:   ESP32-C3 + ADC-Regensensor an GPIO3
 * Pin-Belegung (siehe PinConfig.h):
 *   - Regensensor (ADC): GPIO3 (0-4095)
 *   - Batterie-ADC:       HardwarePinStandard::PIN_BATTERY_ADC
 *   - Setup-Button:       GPIO2 (active-LOW)
 *   - Setup-LED:          GPIO7 (active-HIGH)
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 *
 * @note BatSenRuntime-Basistyp ruft die Custom-Hooks auf (ESP-NOW + MQTT).
 */

#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
using SmartHome::absDiffU16;

#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 1
#include "../../basetypes/bat_sen/BatSenRuntime.h"

// =============================================================================
// COMPILEZEIT-VALIDIERUNG
// =============================================================================

static_assert(BAT_SEN_RAIN_SIGNAL_PIN >= 0,
    "bat_sen_rain braucht einen gueltigen ADC-Pin.");
static_assert(BAT_SEN_RAIN_STATE_DELTA_RAW > 0U,
    "BAT_SEN_RAIN_STATE_DELTA_RAW muss groesser als 0 sein.");

#if BAT_SEN_RAIN_LEVEL_HIGH_IS_WET
static_assert(
    BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW <= BAT_SEN_RAIN_WET_THRESHOLD_RAW,
    "Bei HIGH=wet muss CLEAR <= WET sein.");
#else
static_assert(
    BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW >= BAT_SEN_RAIN_WET_THRESHOLD_RAW,
    "Bei LOW=wet muss CLEAR >= WET sein.");
#endif

// =============================================================================
// LOKALER ZUSTAND – Regen-Status, Event-Flags, Timer
// =============================================================================

namespace {
bool regen_erkannt = false;         // true = aktueller Status ist "nass"
bool event_pending = false;         // true = Statuswechsel noch nicht gemeldet
bool rain_init_ok = false;          // true = GPIO-Init erfolgreich
uint16_t rain_raw = 0U;             // Letzter ADC-Rohwert (0-4095)
bool event_regenstatus = false;     // Gemerkter Event-Status (nass/trocken)
uint16_t event_raw = 0U;            // Gemerkter Event-Rohwert
unsigned long letzte_probe_ms = 0UL; // Zeitstempel letzte ADC-Messung

/** @brief Liest den ADC-Rohwert vom Regensensor-Pin (0-4095, 12-Bit). */
uint16_t leseRainRaw() {
    int raw = analogRead(BAT_SEN_RAIN_SIGNAL_PIN);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

/**
 * @brief Bestimmt, ob ein ADC-Rohwert "Regen" (nass) bedeutet – mit Hysterese.
 *
 * Zwei Schwellen verhindern Flattern:
 * - Bei bisher "trocken": Rohwert muss Wet-Schwelle ueberschreiten fuer "nass"
 * - Bei bisher "nass":    Rohwert muss Clear-Schwelle unterschreiten fuer "trocken"
 *
 * Die Schwellen-Richtung wird via BAT_SEN_RAIN_LEVEL_HIGH_IS_WET
 * zur Compile-Zeit konfiguriert.
 *
 * @param raw          ADC-Rohwert (0-4095)
 * @param bisherRegen  true wenn der letzte bekannte Status "nass" war
 * @return true wenn der Rohwert "nass" bedeutet
 */
bool istRegenZustand(uint16_t raw, bool bisherRegen) {
#if BAT_SEN_RAIN_LEVEL_HIGH_IS_WET
    if (bisherRegen) {
        return raw >= BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW;
    }
    return raw >= BAT_SEN_RAIN_WET_THRESHOLD_RAW;
#else
    if (bisherRegen) {
        return raw <= BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW;
    }
    return raw <= BAT_SEN_RAIN_WET_THRESHOLD_RAW;
#endif
}
}  // namespace

// =============================================================================
// CUSTOM-DEVICE-HOOKS – Werden vom BatSenRuntime-Basistyp aufgerufen
// =============================================================================

/**
 * @brief Initialisiert den ADC-Pin, fuehrt erste Messung durch und setzt den lokalen Zustand.
 *
 * Wird einmalig von BatSenRuntime beim Boot aufgerufen.
 * Setzt regen_erkannt, rain_raw, rain_init_ok.
 */
void device_init_io() {
    pinMode(BAT_SEN_RAIN_SIGNAL_PIN, INPUT);
    // ADC-Pin konfigurieren (ESP32 Arduino 3.x: analogReadPin nicht mehr nötig)
    // Attenuation wird in ESP32 Arduino 3.x automatisch gesetzt

    rain_raw = leseRainRaw();
    regen_erkannt = istRegenZustand(rain_raw, false);
    event_pending = false;
    event_regenstatus = regen_erkannt;
    event_raw = rain_raw;
    letzte_probe_ms = millis();
    rain_init_ok = true;

    logf("INFO", "Rain init: pin=%d raw=%u status=%s",
         BAT_SEN_RAIN_SIGNAL_PIN, rain_raw,
         regen_erkannt ? "wet" : "dry");
}

/**
 * @brief Periodische ADC-Messung mit Hysterese und Event-Erkennung.
 *
 * Wird von BatSenRuntime im Loop aufgerufen. Misst nur wenn das
 * Sample-Intervall abgelaufen ist. Erkennt Statuswechsel (nass<->trocken)
 * und setzt event_pending.
 *
 * @return true wenn sich Werte signifikant geaendert haben → neuer STATE noetig
 */
bool device_poll_inputs() {
    if (!rain_init_ok) return false;

    const unsigned long jetzt = millis();
    // Prueft ob Sample-Intervall (200ms) abgelaufen
    if ((jetzt - letzte_probe_ms) < BAT_SEN_RAIN_SAMPLE_INTERVAL_MS) {
        return false;
    }
    letzte_probe_ms = jetzt;

    const uint16_t neuerRaw = leseRainRaw();
    const bool neuerRegenstatus = istRegenZustand(neuerRaw, regen_erkannt);
    const bool statusGeaendert = neuerRegenstatus != regen_erkannt;
    const bool rawGeaendert = absDiffU16(neuerRaw, rain_raw) >= BAT_SEN_RAIN_STATE_DELTA_RAW;

    if (!statusGeaendert && !rawGeaendert) {
        return false;
    }

    rain_raw = neuerRaw;

    // Statuswechsel: Event merken
    if (statusGeaendert) {
        regen_erkannt = neuerRegenstatus;
        event_pending = true;
        event_regenstatus = neuerRegenstatus;
        event_raw = neuerRaw;
        logf("INFO", "Rain status geaendert: %s (raw=%u)",
             regen_erkannt ? "wet" : "dry", rain_raw);
    }

    return true;
}

/**
 * @brief Befuellt die generischen Zustands-Channel aus den lokalen Regen-Daten.
 *
 * @param[out] channelBool1  regen_erkannt (1=nass, 0=trocken)
 * @param[out] channelU16_1  rain_raw (aktueller ADC-Rohwert)
 * @param[out] channelMask1  immer 0 (keine Maske)
 * @param[out] fault         true wenn rain_init_ok == false
 */
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = regen_erkannt ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = rain_raw;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !rain_init_ok;
}

/**
 * @brief Erzeugt ein Regen-Event (SH_EVENT_RAIN_DETECTED) bei Statuswechsel.
 *
 * Wird nur aufgerufen wenn event_pending == true. Setzt event_pending zurueck.
 *
 * @param[out] eventType  SH_EVENT_RAIN_DETECTED
 * @param[out] trigger    SH_TRIGGER_AUTO
 * @param[out] param1     1 bei "nass", 0 bei "trocken"
 * @param[out] param2     ADC-Rohwert zum Zeitpunkt der Erkennung
 * @return true wenn ein Event bereitsteht
 */
bool device_map_event(
    uint8_t* eventType, uint8_t* trigger,
    uint8_t* param1, uint16_t* param2)
{
    if (!event_pending) return false;
    event_pending = false;

    if (eventType != nullptr) *eventType = SH_EVENT_RAIN_DETECTED;
    if (trigger != nullptr) *trigger = SH_TRIGGER_AUTO;
    if (param1 != nullptr) *param1 = event_regenstatus ? 1U : 0U;
    if (param2 != nullptr) *param2 = event_raw;
    return true;
}

/**
 * @brief Liefert potenzielle GPIO-Wake-Pins fuer Deep-Sleep.
 * @return Bitmaske der Wake-Pins (0 wenn GPIO-Wake deaktiviert)
 */
uint64_t device_wake_candidates() {
#if BAT_SEN_ENABLE_GPIO_WAKE
    if (BAT_SEN_RAIN_SIGNAL_PIN < 0 || BAT_SEN_RAIN_SIGNAL_PIN >= 64) return 0ULL;
    return (1ULL << (uint8_t)BAT_SEN_RAIN_SIGNAL_PIN);
#else
    return 0ULL;
#endif
}
