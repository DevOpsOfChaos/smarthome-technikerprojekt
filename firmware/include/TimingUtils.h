// =============================================================================
// TimingUtils.h – Gemeinsame Zeitgeber und Entprell-Logik
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/include/TimingUtils.h
// Teil des:   Baukastensystems Block 2 – Gemeinsame Services
//
// Verwendung: #include "TimingUtils.h" in jedem Device-main.cpp
//             Alle Funktionen inline, Klassen als Header-only.
//
// Enthält:
//   - elapsedSince: millis()-wrap-sichere Zeitdifferenz
//   - intervalElapsed: Periodische-Ausführung-Prüfung
//   - Debouncer: Entprellung digitaler Eingänge (Taster, Kontakte, PIR)
//   - LongPressDetector: Langdruck-Erkennung (Setup-Taster, Reset)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#pragma once
#include <stdint.h>

namespace SmartHome {

// =============================================================================
// MILLIS()-DIFFERENZ
// =============================================================================

// elapsedSince – Berechnet millis()-wrap-sichere Zeitdifferenz.
//   Der ESP32-millis()-Zähler läuft nach ~49 Tagen über (unsigned long wrap).
//   Dank unsigned-Arithmetik gilt: (nowMs - timestamp) liefert auch bei Wrap
//   das korrekte Delta. Kein Sonderfall-Handling nötig!
//
//   Beispiel: timestamp = 4294967290 (kurz vor Wrap), nowMs = 500 (nach Wrap)
//             elapsedSince → 500 - 4294967290 = 506 (korrekt, unsigned wrap)
inline unsigned long elapsedSince(unsigned long timestampMs, unsigned long nowMs) {
    return nowMs - timestampMs;
}

// intervalElapsed – Prüft, ob ein Intervall seit dem letzten Zeitstempel abgelaufen ist.
//   timestampMs: letzter Ausführungszeitpunkt (0 = erster Aufruf → immer true)
//   nowMs:       aktueller millis()-Wert
//   intervalMs:  gewünschter Mindestabstand
//
//   Typische Verwendung:
//     static unsigned long lastPoll = 0;
//     if (intervalElapsed(lastPoll, millis(), 5000)) {
//       lastPoll = millis();
//       pollSensors();
//     }
inline bool intervalElapsed(unsigned long timestampMs, unsigned long nowMs,
                             unsigned long intervalMs) {
    if (timestampMs == 0UL) return true;
    return (nowMs - timestampMs) >= intervalMs;
}

// =============================================================================
// DEBOUNCER – Entprellung digitaler Eingangssignale
// =============================================================================
// Ein Pegelwechsel an digitalen Eingängen (Taster, Fensterkontakt, PIR)
// wird erst nach einer stabilen Wartezeit (Debounce-Zeit) als gültig
// akzeptiert. Verhindert Mehrfach-Auslösung durch Prellen.
//
// Verwendung:
//   Debouncer btn(40);  // 40ms Entprellzeit
//   void loop() {
//     unsigned long now = millis();
//     bool pressed = btn.update(digitalRead(PIN), now);
//     if (btn.changed() && pressed) {
//       // Gültige steigende Flanke erkannt
//     }
//   }
class Debouncer {
public:
    // debounceMs: Entprellzeit in Millisekunden (Standard: 40ms für Taster)
    explicit Debouncer(unsigned long debounceMs = 40UL)
        : debounceMs_(debounceMs), stable_(false), lastRaw_(false),
          lastStable_(false), changed_(false), lastChangeMs_(0UL) {}

    // update – Rohwert einlesen, entprellten Zustand zurückgeben.
    //   raw:   aktueller digitalRead()-Wert (true/false)
    //   nowMs: aktueller millis()-Zeitstempel
    bool update(bool raw, unsigned long nowMs) {
        changed_ = false;
        if (raw != lastRaw_) {
            lastRaw_ = raw;
            lastChangeMs_ = nowMs;
        }
        if ((nowMs - lastChangeMs_) >= debounceMs_) {
            bool newStable = lastRaw_;
            if (newStable != lastStable_) {
                changed_ = true;
                lastStable_ = newStable;
            }
            stable_ = newStable;
        }
        return stable_;
    }

    // changed – War der letzte update()-Aufruf ein gültiger Pegelwechsel?
    bool changed() const { return changed_; }

    // isActive – Aktueller entprellter Zustand.
    bool isActive() const { return stable_; }

private:
    unsigned long debounceMs_;
    bool stable_, lastRaw_, lastStable_, changed_;
    unsigned long lastChangeMs_;
};

// =============================================================================
// LONG-PRESS-DETECTOR – Erkennung von langem Tastendruck
// =============================================================================
// Erkennt, wenn ein Taster für eine konfigurierbare Mindestdauer gehalten wird.
// Löst nur EINMAL pro Haltezyklus aus (kein Dauerfeuer).
//
// Verwendung (Setup-Taster, 5 Sekunden):
//   LongPressDetector setupBtn(5000);
//   void loop() {
//     unsigned long now = millis();
//     bool active = !digitalRead(PIN);  // active-LOW
//     if (setupBtn.update(active, now) && setupBtn.justTriggered()) {
//       enterSetupMode();
//     }
//   }
class LongPressDetector {
public:
    // holdMs: erforderliche Haltezeit in Millisekunden (Standard: 5000ms)
    explicit LongPressDetector(unsigned long holdMs = 5000UL)
        : holdMs_(holdMs), pressStartMs_(0UL), held_(false), consumed_(false) {}

    // update – Taster-Zustand übergeben.
    //   active: true = Taster aktuell gedrückt
    //   nowMs:  aktueller millis()-Wert
    //   Rückgabe: true wenn long-press-Bedingung erfüllt
    bool update(bool active, unsigned long nowMs) {
        if (!active) {
            // Losgelassen → alles zurücksetzen für nächsten Zyklus
            pressStartMs_ = 0UL;
            consumed_ = false;
            held_ = false;
            return false;
        }
        // Gedrückt – Zeitstempel merken (nur beim ersten Mal)
        if (pressStartMs_ == 0UL) {
            pressStartMs_ = nowMs;
        }
        // Langdruck erkannt? Nur einmal pro Haltezyklus auslösen
        if (!consumed_ && (nowMs - pressStartMs_) >= holdMs_) {
            held_ = true;
            consumed_ = true;
            return true;
        }
        held_ = (nowMs - pressStartMs_) >= holdMs_;
        return held_;
    }

    // justTriggered – Wurde der Langdruck GERADE erkannt? (einmalig, Flanke)
    bool justTriggered() const { return held_ && consumed_; }

    // isHeld – Wird der Taster aktuell lange gehalten? (Zustand, nicht Flanke)
    bool isHeld() const { return held_ && !consumed_; }

private:
    unsigned long holdMs_, pressStartMs_;
    bool held_, consumed_;
};

} // namespace SmartHome
