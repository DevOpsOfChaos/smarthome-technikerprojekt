/*
===============================================================================
 Datei: main.cpp
 Code-Name: BAT-SEN Rain
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Batteriebetriebener Regensensor mit analogem Messwert
 Beschreibung: Liest einen ADC-Regensensor, bewertet den Rohwert mit Hysterese
 und meldet Statuswechsel zwischen "nass" und "trocken" als Event. Zwischen den
 Messungen nutzt der Batterie-Basistyp Deep-Sleep. Das regulaere Timer-Wakeup
 erfolgt alle 15 Minuten; das entspricht 900 Sekunden.

 Hardware:
 - ESP32-C3
 - ADC-Regensensor an GPIO3, Messbereich 0 bis 4095 bei 12 Bit ADC-Aufloesung
 - Batterie-ADC ueber HardwarePinStandard::PIN_BATTERY_ADC
 - Setup-Button an GPIO2, active-LOW
 - Setup-LED an GPIO7, active-HIGH

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Funktionen wie pinMode, analogRead und millis.
 - DeviceConfig.h: eigene Device-Konfiguration mit ADC-Pin, Schwellen und Zeiten.
 - PinConfig.h: eigene Pin-Zuordnung fuer diesen konkreten Node.
 - MathUtils.h: eigene Hilfsbibliothek; absDiffU16 erkennt relevante ADC-Aenderungen.
 - BatSenRuntime.h: eigener Batterie-Sensor-Basistyp; liefert setup(), loop(),
   Funkprotokoll, Schlaflogik und ruft die Device-Hooks aus dieser Datei auf.

 Aenderungsverlauf:
 - 2026-05-14: Device-Code fuer Batterie-Regensensor angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
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
    "bat_sen_rain_sensor braucht einen gueltigen ADC-Pin.");
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

// Aufgabe: Liest den ADC-Rohwert vom Regensensor-Pin.
// Eingabewerte: keine; der ADC-Pin kommt aus DeviceConfig.h.
// Ausgabewert: Rohwert von 0 bis 4095. 4095 ist der Maximalwert bei 12 Bit.
uint16_t leseRainRaw() {
    int raw = analogRead(BAT_SEN_RAIN_SIGNAL_PIN);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

// Aufgabe: Bewertet einen ADC-Rohwert als "nass" oder "trocken".
// Eingabewerte:
// - raw: ADC-Rohwert von 0 bis 4095.
// - bisherRegen: true, wenn der letzte stabile Zustand bereits "nass" war.
// Ausgabewert: true bedeutet "nass", false bedeutet "trocken".
//
// Zwei Schwellen bilden eine Hysterese und verhindern Flattern am Grenzwert:
// Bei bisher "trocken" muss die Wet-Schwelle erreicht werden.
// Bei bisher "nass" muss erst die Clear-Schwelle erreicht werden.
// BAT_SEN_RAIN_LEVEL_HIGH_IS_WET legt fest, ob hohe oder niedrige Rohwerte nass bedeuten.
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
// CUSTOM-DEVICE-HOOKS - Werden vom BatSenRuntime-Basistyp aufgerufen
// =============================================================================

// Aufgabe: Initialisiert den ADC-Pin, fuehrt die erste Messung aus und setzt den lokalen Zustand.
// Eingabewerte: keine; Pin und Schwellwerte kommen aus DeviceConfig.h.
// Ausgabewert: keiner; regen_erkannt, rain_raw und rain_init_ok werden gesetzt.
// Aufrufer: BatSenRuntime ruft diesen Hook einmal beim Boot auf.
void device_init_io() {
    pinMode(BAT_SEN_RAIN_SIGNAL_PIN, INPUT);
    // ADC-Pin konfigurieren. ESP32 Arduino 3.x setzt die Attenuation automatisch.

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

// Aufgabe: Fuehrt eine periodische ADC-Messung aus und erkennt relevante Aenderungen.
// Eingabewerte: keine; der aktuelle Zeitpunkt kommt aus millis().
// Ausgabewert: true bedeutet, ein neuer STATE-Report ist sinnvoll.
//
// BAT_SEN_RAIN_SAMPLE_INTERVAL_MS ist eine Millisekunden-Zeit. Beispiel:
// 200UL bedeutet 200 Millisekunden zwischen zwei ADC-Proben. BAT_SEN_RAIN_STATE_DELTA_RAW
// ist die minimale Rohwertdifferenz, ab der ein neuer STATE gesendet wird.
bool device_poll_inputs() {
    if (!rain_init_ok) return false;

    const unsigned long jetzt = millis();
    // Prueft, ob das Sample-Intervall in Millisekunden abgelaufen ist.
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

    // Statuswechsel: Event merken; das eigentliche Senden erledigt der Basistyp.
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

// Aufgabe: Uebergibt den Regenstatus an die generischen Kanaele des Basistyps.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr wird ignoriert.
// Ausgabewerte:
// - channelBool1: 1 bedeutet nass, 0 bedeutet trocken.
// - channelU16_1: aktueller ADC-Rohwert von 0 bis 4095.
// - channelMask1: hier ungenutzt, bleibt 0.
// - fault: true bedeutet, die ADC-Initialisierung war nicht erfolgreich.
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = regen_erkannt ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = rain_raw;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !rain_init_ok;
}

// Aufgabe: Wandelt einen gemerkten Regen-Statuswechsel in ein Protokoll-Event um.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr wird ignoriert.
// Ausgabewerte:
// - eventType: SH_EVENT_RAIN_DETECTED aus Protocol.h.
// - trigger: SH_TRIGGER_AUTO, weil der Sensor das Event selbst erkannt hat.
// - param1: 1 bei "nass", 0 bei "trocken".
// - param2: ADC-Rohwert zum Zeitpunkt der Erkennung.
// Rueckgabe: true bedeutet, ein Event wurde bereitgestellt.
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

// Aufgabe: Meldet dem Basistyp, welcher GPIO das Geraet aus Deep-Sleep wecken darf.
// Eingabewerte: keine; BAT_SEN_RAIN_SIGNAL_PIN kommt aus DeviceConfig.h.
// Ausgabewert: Bitmaske mit gesetztem Pin-Bit oder 0, wenn GPIO-Wake aus ist.
uint64_t device_wake_candidates() {
#if BAT_SEN_ENABLE_GPIO_WAKE
    if (BAT_SEN_RAIN_SIGNAL_PIN < 0 || BAT_SEN_RAIN_SIGNAL_PIN >= 64) return 0ULL;
    return (1ULL << (uint8_t)BAT_SEN_RAIN_SIGNAL_PIN);
#else
    return 0ULL;
#endif
}
