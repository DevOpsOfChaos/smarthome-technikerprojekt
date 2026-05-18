/*
===============================================================================
 Datei: AdcUtils.h
 Code-Name: AdcUtils
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: ADC-Hilfsfunktionen fuer Batteriegeraete
 Beschreibung: Liest ADC-Werte sicher ein und verhindert Zustandsflattern an analogen Schaltschwellen.

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-/ESP32-Grundfunktionen wie analogRead().

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/
#pragma once
#include <Arduino.h>

namespace SmartHome {

// =============================================================================
// ADC-READ MIT BEREICHSPRÜFUNG
// =============================================================================

// adcReadClamped - Liest ADC-Pin und begrenzt auf gültigen Bereich [0, 4095].
//   Der ESP32-C3 hat einen 12-Bit-ADC (0-4095). Durch Rauschen oder
//   fehlerhafte Konfiguration können Werte knapp unter 0 oder über 4095
//   auftreten. Diese Funktion fängt das ab.
//
//   pin: GPIO-Pin-Nummer (muss per adcAttachPin() vorbereitet sein)
//   Rückgabe: ADC-Rohwert, garantiert im Bereich [0, 4095]
inline uint16_t adcReadClamped(uint8_t pin) {
    int raw = analogRead(pin);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

// =============================================================================
// HYSTERESE-FILTER FÜR ADC-BASIERTE ZUSTÄNDE
// =============================================================================
// Verhindert Flattern an Schaltschwellen durch zwei unterschiedliche Pegel.
// Beispiel Regensensor:
//   "trocken" -> "nass": erst ab ADC > 2200 (wet_threshold)
//   "nass" -> "trocken": erst ab ADC < 2050 (clear_threshold)
//   Ohne Hysterese würde der Wert bei ~2100 ständig hin- und herspringen.

// adcApplyHysteresis - Hysteresefilter für boolesche ADC-Zustände.
//   currentState: Referenz auf aktuellen Zustand (wird bei Wechsel aktualisiert)
//   raw:          aktueller ADC-Rohwert
//   highThresh:   Schwelle für Übergang false -> true
//   lowThresh:    Schwelle für Übergang true  -> false
//   highIsTrue:   true = hoher ADC-Wert bedeutet "true" (z.B. nass bei Leitfähigkeit)
//
//   Rückgabe: true wenn sich currentState geändert hat
//
//   Beispiel:
//     bool istNass = false;
//     if (adcApplyHysteresis(istNass, adcReadClamped(PIN), 2200, 2050, true)) {
//       logf("Regenstatus geaendert: %s", istNass ? "nass" : "trocken");
//     }
inline bool adcApplyHysteresis(bool& currentState, uint16_t raw,
                                uint16_t highThresh, uint16_t lowThresh,
                                bool highIsTrue = true) {
    bool newState;
    if (currentState) {
        // Bereits true: zum Zurücksetzen niedrigere Schwelle nutzen
        newState = highIsTrue ? (raw >= lowThresh) : (raw <= lowThresh);
    } else {
        // Noch false: zum Setzen höhere Schwelle nutzen
        newState = highIsTrue ? (raw >= highThresh) : (raw <= highThresh);
    }
    if (newState == currentState) return false;
    currentState = newState;
    return true;
}

} // namespace SmartHome
