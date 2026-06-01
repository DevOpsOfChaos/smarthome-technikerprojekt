/*
===============================================================================
 Datei: main.cpp
 Code-Name: BAT-SEN Window Contact
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Batterie-Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Batteriebetriebener Fensterkontakt mit Reed-Kontakt
 Beschreibung: Liest den Reed-Kontakt ein, entprellt Pegelwechsel und meldet
 Statuswechsel als Fenster-Events. Der Basistyp uebernimmt ESP-NOW, MQTT-
 Weitergabe, Batteriespannung, Setup-Modus und Deep-Sleep. Das Geraet wacht
 bei GPIO-Aenderung in beide Richtungen und zusaetzlich per Timer auf;
 15 Minuten bedeuten 900 s = 15 min Wake-Intervall.

 Hardware:
 - ESP32-C3
 - Reed-Kontakt an GPIO3
 - CR2032-Batterie, erwarteter Bereich etwa 2200 bis 3000 mV
 - Setup-Button an GPIO2, active-LOW
 - Setup-LED an GPIO7, active-HIGH

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Funktionen wie pinMode, digitalRead, millis, HIGH und LOW.
 - DeviceConfig.h: eigene Device-Konfiguration mit Kontaktlogik und Intervallen.
 - PinConfig.h: eigene Pin-Zuordnung fuer diesen konkreten Node.
 - BatSenRuntime.h: eigener Batterie-Sensor-Basistyp; liefert setup(), loop(),
   Funkprotokoll, Schlaflogik und ruft die Device-Hooks aus dieser Datei auf.

 Aenderungsverlauf:
 - 2026-05-14: Device-Code fuer Reed-Fensterkontakt angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/

#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 1
#define BAT_SEN_DEVICE_HAS_DYNAMIC_WAKE_LEVEL 1
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
unsigned long letzte_flanke_ms = 0UL;   // Zeitstempel letzter Flankenwechsel

// Aufgabe: Prueft, ob ein gelesener GPIO-Pegel als "Fenster offen" gilt.
// Eingabewert: level ist das Ergebnis von digitalRead(), also HIGH oder LOW.
// Ausgabewert: true bedeutet "offen", false bedeutet "geschlossen".
// Hinweis: BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH legt die Wirkrichtung fest.
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

// Aufgabe: Initialisiert den Reed-Kontakt-Pin und liest den ersten Zustand.
// Eingabewerte: keine; Pin und Pullup-Verhalten kommen aus DeviceConfig.h.
// Ausgabewert: keiner; lokale Statusvariablen werden fuer den ersten Report gesetzt.
// Aufrufer: BatSenRuntime ruft diesen Hook einmal beim Boot auf.
void device_init_io() {
#if BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT_PULLUP);
#else
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT);
#endif

    stable_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);
    last_raw_level = stable_level;
    letzte_flanke_ms = millis();
    kontakt_offen = levelIstOffen(stable_level);
    event_pending = false;
    kontakt_init_ok = true;

    logf("INFO", "Window-Kontakt init: pin=%d status=%s",
         BAT_SEN_WINDOW_CONTACT_PIN,
         kontakt_offen ? "open" : "closed");
}

// Aufgabe: Liest den Reed-Kontakt entprellt und erkennt echte Statuswechsel.
// Eingabewerte: keine; der aktuelle Pegel kommt per digitalRead() vom GPIO.
// Ausgabewert: true bedeutet, der Fensterstatus hat sich geaendert und ein
// STATE-Report ist sinnvoll. false bedeutet, es gibt nichts Neues.
//
// BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS = 50 ms Entprellzeit (verhindert falsche Events durch Kontaktprellen).
bool device_poll_inputs() {
    if (!kontakt_init_ok) return false;

    const unsigned long jetzt = millis();
    const int raw_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);

    // Rohwert geaendert: Entprell-Timer neu starten.
    if (raw_level != last_raw_level) {
        last_raw_level = raw_level;
        letzte_flanke_ms = jetzt;
        return false;
    }

    // Entprellzeit ist noch nicht abgelaufen; der Pegel ist noch nicht sicher.
    if ((jetzt - letzte_flanke_ms) < BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS) {
        return false;
    }

    // Pegel ist gleich wie beim letzten stabilen Stand; kein neues Ereignis.
    if (raw_level == stable_level) {
        return false;
    }

    // Neuer stabiler Pegel: jetzt erst wird daraus ein Fensterstatus.
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

// Aufgabe: Uebergibt den Fensterstatus an die generischen Kanaele des Basistyps.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr wird ignoriert.
// Ausgabewerte:
// - channelBool1: 1 bedeutet offen, 0 bedeutet geschlossen.
// - channelU16_1: hier ungenutzt, bleibt 0.
// - channelMask1: hier ungenutzt, bleibt 0.
// - fault: true bedeutet, die GPIO-Initialisierung war nicht erfolgreich.
void device_build_state_channels(
    uint8_t* channelBool1, uint16_t* channelU16_1,
    uint8_t* channelMask1, bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = kontakt_offen ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = 0U;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !kontakt_init_ok;
}

// Aufgabe: Wandelt einen gemerkten Statuswechsel in ein Protokoll-Event um.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr wird ignoriert.
// Ausgabewerte:
// - eventType: SH_EVENT_WINDOW_OPENED oder SH_EVENT_WINDOW_CLOSED aus Protocol.h.
// - trigger: SH_TRIGGER_AUTO, weil der Sensor das Event selbst erkannt hat.
// - param1: 1 bei "offen", 0 bei "geschlossen".
// - param2: Rohinformation zum stabilen Pegel; 1 bei HIGH, 0 bei LOW.
// Rueckgabe: true bedeutet, ein Event wurde bereitgestellt.
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

// Aufgabe: Meldet dem Basistyp, welcher GPIO das Geraet aus Deep-Sleep wecken darf.
// Eingabewerte: keine; BAT_SEN_WINDOW_CONTACT_PIN kommt aus DeviceConfig.h.
// Ausgabewert: Bitmaske mit gesetztem Pin-Bit oder 0 bei ungueltigem GPIO.
uint64_t device_wake_candidates() {
    if (BAT_SEN_WINDOW_CONTACT_PIN < 0 || BAT_SEN_WINDOW_CONTACT_PIN >= 64) {
        return 0ULL;
    }
    return (1ULL << (uint8_t)BAT_SEN_WINDOW_CONTACT_PIN);
}

// Aufgabe: Waehlt vor dem Deep-Sleep den naechsten Wake-Pegel passend zum
// aktuellen Reed-Zustand. So weckt geschlossen->offen und offen->geschlossen
// jeweils sofort, obwohl ESP32-C3 GPIO-Wake level-basiert ist.
bool device_wake_level_high() {
    return stable_level == LOW;
}
