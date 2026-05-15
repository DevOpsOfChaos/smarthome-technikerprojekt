/**
 * @file main.cpp
 * @brief BAT-SEN Window Contact: Batterie-Fensterkontakt mit Reed-Kontakt
 *
 * @details GPIO-Wake bei Pegelwechsel (Fenster auf/zu) + Timer-Wake alle 15 Min.
 *          Entprellung via BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS.
 *          Event bei Statuswechsel: SH_EVENT_WINDOW_OPENED / SH_EVENT_WINDOW_CLOSED.
 *
 * Hardware:   ESP32-C3 + Reed-Kontakt an GPIO3
 * Batterie:   CR2032 (2200-3000mV)
 * Pin-Belegung:
 *   - Reed-Kontakt: GPIO3 (INPUT_PULLUP, offen=HIGH)
 *   - Setup-Button: GPIO2 (active-LOW)
 *   - Setup-LED:    GPIO7 (active-HIGH)
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 */

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

/**
 * @brief Prueft ob ein GPIO-Pegel "Fenster offen" bedeutet.
 * @param level digitalRead-Ergebnis (HIGH oder LOW)
 * @return true wenn der Pegel als "offen" interpretiert wird
 * @note Die Richtung wird via BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH konfiguriert.
 */
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

/**
 * @brief Initialisiert den Reed-Kontakt-Pin mit Pullup und liest den ersten Zustand.
 * Wird einmalig von BatSenRuntime beim Boot aufgerufen.
 */
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

/**
 * @brief Entprellte Kontakt-Abfrage mit Statuswechsel-Erkennung.
 *
 * Wartet BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS nach letztem Flankenwechsel,
 * bevor der Pegel als stabil uebernommen wird. Setzt event_pending bei Wechsel.
 *
 * @return true wenn sich der Status geaendert hat
 */
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

/**
 * @brief Befuellt die generischen Zustands-Channel aus den Fensterkontakt-Daten.
 * @param[out] channelBool1  kontakt_offen (1=offen, 0=geschlossen)
 * @param[out] channelU16_1  immer 0
 * @param[out] channelMask1  immer 0
 * @param[out] fault         true wenn kontakt_init_ok == false
 */
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = kontakt_offen ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = 0U;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !kontakt_init_ok;
}

/**
 * @brief Erzeugt ein Fenster-Event bei Statuswechsel.
 *
 * @param[out] eventType  SH_EVENT_WINDOW_OPENED oder SH_EVENT_WINDOW_CLOSED
 * @param[out] trigger    SH_TRIGGER_AUTO
 * @param[out] param1     1 bei "offen", 0 bei "geschlossen"
 * @param[out] param2     1 wenn stable_level == HIGH, sonst 0
 * @return true wenn ein Event bereitsteht (event_pending war true)
 */
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

/**
 * @brief Registriert den Reed-Kontakt-Pin als GPIO-Wake-Quelle.
 * @return Bitmaske mit Bit BAT_SEN_WINDOW_CONTACT_PIN gesetzt
 */
uint64_t device_wake_candidates() {
    if (BAT_SEN_WINDOW_CONTACT_PIN < 0 || BAT_SEN_WINDOW_CONTACT_PIN >= 64) {
        return 0ULL;
    }
    return (1ULL << (uint8_t)BAT_SEN_WINDOW_CONTACT_PIN);
}
