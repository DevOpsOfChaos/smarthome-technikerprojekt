// =============================================================================
// main.cpp – BAT-SEN Rain: Batteriebetriebener Regensensor
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_rain/main.cpp
// Hardware:   ESP32-C3 + ADC-Regensensor an GPIO3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung (siehe PinConfig.h fuer Details):
//   Regensensor (ADC): GPIO3 – Analog-Signal (0-4095)
//   Batterie-ADC:       HardwarePinStandard::PIN_BATTERY_ADC
//   Setup-Button:       GPIO2 (active-LOW)
//   Setup-LED:          GPIO7 (active-HIGH)
//
// Funktionsweise:
//   Timer-Wake alle 15 Min, ADC-Messung mit Hysterese (2050/2200),
//   Event bei Zustandswechsel (nass/trocken), Deep-Sleep dazwischen.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

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

// leseRainRaw – Liest ADC-Rohwert vom Regensensor-Pin (0-4095, 12-Bit)
uint16_t leseRainRaw() {
    int raw = analogRead(BAT_SEN_RAIN_SIGNAL_PIN);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

// istRegenZustand – Bestimmt ob Rohwert "nass" bedeutet (mit Hysterese)
//   Bei bisher "trocken": Wet-Schwelle (2200) ueberschreiten fuer "nass"
//   Bei bisher "nass":   Clear-Schwelle (2050) unterschreiten fuer "trocken"
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

// device_init_io – GPIO-Initialisierung: ADC-Pin, erste Messung, Log
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

// device_poll_inputs – Periodische ADC-Messung mit Hysterese
//   Parameter: keine
//   Rückgabe: true = Werte haben sich geaendert (neuer STATE noetig)
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

// device_build_state_channels – Baut generische Zustaende aus den Regen-Daten
//   channelBool1 = regen_erkannt (1=nass, 0=trocken)
//   channelU16_1 = rain_raw (aktueller ADC-Rohwert)
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = regen_erkannt ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = rain_raw;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !rain_init_ok;
}

// device_map_event – Erzeugt ein Regen-Event bei Zustandswechsel
//   event_type = SH_EVENT_RAIN_DETECTED
//   param1 = 1 bei "nass", 0 bei "trocken"
//   param2 = Rohwert der ersten Erkennung
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

// device_wake_candidates – Liefert potenzielle GPIO-Wake-Pins (hier: kein GPIO-Wake)
uint64_t device_wake_candidates() {
#if BAT_SEN_ENABLE_GPIO_WAKE
    if (BAT_SEN_RAIN_SIGNAL_PIN < 0 || BAT_SEN_RAIN_SIGNAL_PIN >= 64) return 0ULL;
    return (1ULL << (uint8_t)BAT_SEN_RAIN_SIGNAL_PIN);
#else
    return 0ULL;
#endif
}
