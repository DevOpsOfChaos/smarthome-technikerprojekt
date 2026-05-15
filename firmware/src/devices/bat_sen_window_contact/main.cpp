// =============================================================================
// main.cpp – BAT-SEN Window Contact: Batterie-Fensterkontakt
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/bat_sen_window_contact/main.cpp
// Hardware:   ESP32-C3 + Reed-Kontakt an GPIO3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung (siehe PinConfig.h fuer Details):
//   Reed-Kontakt: GPIO3 – INPUT_PULLUP, offen=HIGH
//   Batterie:      CR2032 (2200-3000mV)
//   Setup-Button:  GPIO2 (active-LOW)
//   Setup-LED:     GPIO7 (active-HIGH)
//
// Funktionsweise:
//   GPIO-Wake bei Pegelwechsel (Fenster auf/zu) + Timer-Wake alle 15 Min.
//   Entprellung 35ms. Event bei Statuswechsel (SH_EVENT_WINDOW_OPENED/CLOSED).
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 1
#include "../../basetypes/bat_sen/BatSenRuntime.h"

// =============================================================================
// LOKALER ZUSTAND – Kontakt-Status, Entprellung, Event-Flags
// =============================================================================

namespace {
bool kontakt_offen = false;         // true = Fenster geoeffnet
bool event_pending = false;         // true = Statuswechsel nicht gemeldet
bool kontakt_init_ok = false;       // true = GPIO-Init erfolgreich
int last_raw_level = LOW;           // Letzter Rohwert (entprellt)
int stable_level = LOW;             // Stabiler Pegel nach Entprellung
unsigned long last_edge_ms = 0UL;   // Zeitstempel letzter Flankenwechsel

// levelIstOffen – Prueft ob ein GPIO-Pegel "Fenster offen" bedeutet
bool levelIstOffen(int level) {
#if BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH
    return level == HIGH;
#else
    return level == LOW;
#endif
}
}  // namespace

// =============================================================================
// CUSTOM-DEVICE-HOOKS
// =============================================================================

// device_init_io – GPIO-Initialisierung: Pullup, erster Zustand, Log
void device_init_io() {
#if BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT_PULLUP);
#else
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT);
#endif

    stable_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);
    last_raw_level = stable_level;
    last_edge_ms = millis();
    kontakt_offen = levelIstOffen(stable_level);
    event_pending = false;
    kontakt_init_ok = true;

    logf("INFO", "Window-Kontakt init: pin=%d status=%s",
         BAT_SEN_WINDOW_CONTACT_PIN,
         kontakt_offen ? "open" : "closed");
}

// device_poll_inputs – Entprellte Kontakt-Abfrage
//   Wartet BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS (35ms) nach letztem Flankenwechsel.
//   Erst dann wird der Pegel als stabil uebernommen.
//   Rueckgabe: true = Status geaendert
bool device_poll_inputs() {
    if (!kontakt_init_ok) return false;

    const unsigned long jetzt = millis();
    const int raw_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);

    // Rohwert geaendert: Timer zuruecksetzen
    if (raw_level != last_raw_level) {
        last_raw_level = raw_level;
        last_edge_ms = jetzt;
        return false;
    }

    // Entprellzeit noch nicht abgelaufen
    if ((jetzt - last_edge_ms) < BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS) {
        return false;
    }

    // Pegel immer noch gleich wie beim letzten Check → stabil
    if (raw_level == stable_level) {
        return false;
    }

    // Neuer stabiler Pegel erkannt
    stable_level = raw_level;
    const bool neu_offen = levelIstOffen(stable_level);
    if (neu_offen == kontakt_offen) {
        return false;
    }

    kontakt_offen = neu_offen;
    event_pending = true;
    logf("INFO", "Fensterstatus geaendert: %s",
         kontakt_offen ? "open" : "closed");
    return true;
}

// device_build_state_channels – Baut generischen Zustand aus Kontakt-Daten
//   channelBool1 = kontakt_offen (1=offen, 0=geschlossen)
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = kontakt_offen ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = 0U;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !kontakt_init_ok;
}

// device_map_event – Erzeugt Window-Event bei Statuswechsel
//   SH_EVENT_WINDOW_OPENED bei "offen", SH_EVENT_WINDOW_CLOSED bei "zu"
bool device_map_event(
    uint8_t* eventType, uint8_t* trigger,
    uint8_t* param1, uint16_t* param2)
{
    if (!event_pending) return false;
    event_pending = false;

    if (eventType != nullptr) {
        *eventType = kontakt_offen ? SH_EVENT_WINDOW_OPENED : SH_EVENT_WINDOW_CLOSED;
    }
    if (trigger != nullptr) *trigger = SH_TRIGGER_AUTO;
    if (param1 != nullptr) *param1 = kontakt_offen ? 1U : 0U;
    if (param2 != nullptr) *param2 = (uint16_t)((stable_level == HIGH) ? 1U : 0U);
    return true;
}

// device_wake_candidates – GPIO3 als Wake-Quelle registrieren
//   wakeHigh = true (HIGH-Pegel weckt auf)
uint64_t device_wake_candidates() {
    if (BAT_SEN_WINDOW_CONTACT_PIN < 0 || BAT_SEN_WINDOW_CONTACT_PIN >= 64) {
        return 0ULL;
    }
    return (1ULL << (uint8_t)BAT_SEN_WINDOW_CONTACT_PIN);
}
