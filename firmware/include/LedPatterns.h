// =============================================================================
// LedPatterns.h – Standardisierte LED-Blinkmuster
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/include/LedPatterns.h
// Teil des:   Baukastensystems Block 2 – Gemeinsame Services
//
// Verwendung: #include "LedPatterns.h" in jedem Device-main.cpp
//
// Definiert EINHEITLICHE Blinkmuster für Status-LEDs aller Geräte.
// Jedes Gerät im Feld soll die gleichen Muster zeigen, damit der Zustand
// auf den ersten Blick erkennbar ist – unabhängig vom Gerätetyp.
//
// Muster-Übersicht:
//   BOOT       LED aus           WiFi-Verbindungsaufbau läuft
//   ONLINE     LED dauer-an      Normalbetrieb, alles ok
//   SETUP      LED 500ms-Takt    Provisioning-Modus aktiv (von ShNodeProvisioning)
//   WARNING    LED 200ms/800ms   Sensor-Fehler, aber Gerät läuft noch
//   ERROR      LED 100ms/900ms   Kritischer Fehler, Gerät funktionsunfähig
//   OFFLINE    LED 1s/3s         Keine Verbindung zum Master
//
// Die SETUP-LED wird direkt von ShNodeProvisioning gesteuert und ist hier
// nur zur Dokumentation aufgeführt.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#pragma once
#include <stdint.h>

namespace SmartHome {

// =============================================================================
// BLINKMUSTER-KONSTANTEN (Millisekunden)
// =============================================================================
// Diese Konstanten werden als Referenz für die device-spezifische LED-Steuerung
// genutzt. Die tatsächliche Umsetzung erfolgt im Device-Code, da die LED-Hardware
// (GPIO, NeoPixel, aktive High/Low) pro Gerät unterschiedlich ist.

namespace LedPattern {

// Normalbetrieb – LED dauerhaft an (kein Blinken)
constexpr unsigned long ONLINE_ON_MS  = 0UL;
constexpr unsigned long ONLINE_OFF_MS = 0UL;

// Setup-Modus – 500ms an, 500ms aus (von ShNodeProvisioning verwaltet)
constexpr unsigned long SETUP_ON_MS  = 500UL;
constexpr unsigned long SETUP_OFF_MS = 500UL;

// Warnung (Sensor-Teilausfall) – 200ms an, 800ms aus
constexpr unsigned long WARNING_ON_MS  = 200UL;
constexpr unsigned long WARNING_OFF_MS = 800UL;

// Kritischer Fehler – 100ms an, 900ms aus (schnelles Fehlerblinken)
constexpr unsigned long ERROR_ON_MS  = 100UL;
constexpr unsigned long ERROR_OFF_MS = 900UL;

// Offline (kein Master) – 1s an, 3s aus (langsames Suchen)
constexpr unsigned long OFFLINE_ON_MS  = 1000UL;
constexpr unsigned long OFFLINE_OFF_MS = 3000UL;

} // namespace LedPattern

// =============================================================================
// BLINK-STATE-MACHINE
// =============================================================================
// Einfache State-Machine für periodisches LED-Blinken.
// Nutzung:
//   BlinkController led;
//   led.setPattern(LedPattern::ERROR_ON_MS, LedPattern::ERROR_OFF_MS);
//   void loop() {
//     digitalWrite(LED_PIN, led.update(millis()) ? HIGH : LOW);
//   }

class BlinkController {
public:
    BlinkController()
        : onMs_(0UL), offMs_(0UL), state_(false), lastToggleMs_(0UL),
          active_(false) {}

    // setPattern – Blinkmuster setzen.
    //   onMs/offMs beide 0: Dauer-AUS (kein Blinken, kein Leuchten)
    //   onMs=0, offMs>0: Dauer-EIN
    //   onMs>0, offMs=0: Dauer-EIN
    //   onMs>0, offMs>0: Blinken mit angegebenem Takt
    void setPattern(unsigned long onMs, unsigned long offMs) {
        onMs_ = onMs;
        offMs_ = offMs;
        // Bei Musterwechsel sofort neu starten
        state_ = (onMs > 0UL || offMs > 0UL);  // EIN wenn nicht beide 0
        lastToggleMs_ = 0UL;
        active_ = (onMs > 0UL || offMs > 0UL);
    }

    // update – Nächsten Zustand berechnen. Muss in loop() aufgerufen werden.
    //   nowMs: aktueller millis()-Wert
    //   Rückgabe: true = LED soll leuchten, false = LED aus
    bool update(unsigned long nowMs) {
        if (!active_) return false;
        if (onMs_ == 0UL) return true;   // Dauer-EIN
        if (offMs_ == 0UL) return true;  // Dauer-EIN

        if (lastToggleMs_ == 0UL) {
            lastToggleMs_ = nowMs;
            return state_;
        }

        unsigned long interval = state_ ? onMs_ : offMs_;
        if ((nowMs - lastToggleMs_) >= interval) {
            state_ = !state_;
            lastToggleMs_ = nowMs;
        }
        return state_;
    }

    // turnOff – LED dauerhaft ausschalten (z.B. Deep-Sleep)
    void turnOff() {
        active_ = false;
        state_ = false;
    }

private:
    unsigned long onMs_, offMs_, lastToggleMs_;
    bool state_, active_;
};

} // namespace SmartHome
