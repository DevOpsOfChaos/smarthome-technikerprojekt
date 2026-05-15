// =============================================================================
// MathUtils.h – Gemeinsame mathematische Hilfsfunktionen
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/include/MathUtils.h
// Teil des:   Baukastensystems Block 2 – Gemeinsame Services
//
// Verwendung: #include "MathUtils.h" in jedem Device-main.cpp
//             Alle Funktionen sind inline → Header-only, keine .cpp-Datei nötig.
//
// Enthält:
//   - Wertebereich-Begrenzung (Clamping) für uint16, int16, Feuchte
//   - Absolute Differenz (uint16, int16, uint32) – millis()-wrap-sicher
//   - Signifikante-Änderung-Erkennung (Delta-Detection) für uint16/uint32
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#pragma once
#include <stdint.h>

namespace SmartHome {

// =============================================================================
// WERTEBEREICH-BEGRENZUNG (Clamping)
// =============================================================================

// clampToU16 – Begrenzt einen long-Wert auf uint16-Bereich [0, 65535].
//   Negative Werte → 0, Werte > 65535 → 65535.
inline uint16_t clampToU16(long v) {
    return v < 0L ? 0U : v > 65535L ? 65535U : (uint16_t)v;
}

// clampToI16 – Begrenzt einen long-Wert auf int16-Bereich [-32768, 32767].
inline int16_t clampToI16(long v) {
    return v < -32768L ? -32768 : v > 32767L ? 32767 : (int16_t)v;
}

// clampHum01pct – Begrenzt einen long-Wert auf 0.1%-Feuchte-Bereich [0, 1000].
//   1000 = 100.0%, Auflösung 0.1 Prozentpunkte.
inline uint16_t clampHum01pct(long v) {
    return v < 0L ? 0U : v > 1000L ? 1000U : (uint16_t)v;
}

// =============================================================================
// ABSOLUTE DIFFERENZ
// =============================================================================
// Diese Funktionen sind millis()-wrap-sicher, da sie unsigned-Arithmetik nutzen.
// Der ESP32-millis()-Überlauf nach ~49 Tagen wird korrekt behandelt.

// absDiffU16 – Absolute Differenz zweier uint16-Werte (z.B. ADC-Rohwerte).
inline uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return a >= b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

// absDiffI16 – Absolute Differenz zweier int16-Werte, Rückgabe als uint16.
//   Nutzt int32-Hilfsvariable, um Überlauf bei -32768 - 32767 zu vermeiden.
inline uint16_t absDiffI16(int16_t a, int16_t b) {
    const int32_t d = (int32_t)a - (int32_t)b;
    const uint32_t ad = d < 0 ? (uint32_t)(-d) : (uint32_t)d;
    return ad > 65535UL ? 65535U : (uint16_t)ad;
}

// absDiffU32 – Absolute Differenz zweier uint32-Werte (z.B. Zeitstempel).
inline uint32_t absDiffU32(uint32_t a, uint32_t b) {
    return a >= b ? a - b : b - a;
}

// =============================================================================
// DELTA-CHANGE-DETECTION (Signifikante-Änderung-Erkennung)
// =============================================================================
// Prüft, ob sich ein Wert signifikant geändert hat (Differenz >= delta).
// Besonderheit: invalidSentinel-Werte lösen IMMER eine Änderung aus
// (z.B. erster gültiger Wert nach UNGUELTIG, oder gültig → UNGUELTIG).
//
// Spart ESP-NOW-Bandbreite: Nur bei echter Änderung wird ein STATE gesendet.

// valueChangedSignificantU32 – Signifikante Änderung eines uint32-Werts.
//   oldVal: vorheriger Wert (z.B. pressure_pa)
//   newVal: neuer Messwert
//   invalidSentinel: Magic-Number für "ungültig" (z.B. 0xFFFFFFFF)
//   delta: minimale Differenz für "signifikant"
inline bool valueChangedSignificantU32(uint32_t oldVal, uint32_t newVal,
                                        uint32_t invalidSentinel, uint32_t delta) {
    if (oldVal == newVal) return false;
    if (oldVal == invalidSentinel || newVal == invalidSentinel) return true;
    return absDiffU32(oldVal, newVal) >= delta;
}

// valueChangedSignificantU16 – Signifikante Änderung eines uint16-Werts.
//   oldVal: vorheriger Wert (z.B. temp_01c, hum_01pct)
//   newVal: neuer Messwert
//   invalidSentinel: Magic-Number für "ungültig" (z.B. 0xFFFF)
//   delta: minimale Differenz für "signifikant"
inline bool valueChangedSignificantU16(uint16_t oldVal, uint16_t newVal,
                                        uint16_t invalidSentinel, uint16_t delta) {
    if (oldVal == newVal) return false;
    if (oldVal == invalidSentinel || newVal == invalidSentinel) return true;
    return absDiffU16(oldVal, newVal) >= delta;
}

// updateAndCheckU32 – Wert schreiben UND auf signifikante Änderung prüfen.
//   Vereint setzen + change-detection in einem Aufruf (atomar).
//   dest:       Zeiger auf den zu aktualisierenden Wert (nullptr → false)
//   newVal:     neuer Wert
//   invalidSentinel: Magic-Number für "ungültig"
//   delta:      minimale Differenz für "signifikant"
//   Rückgabe:   true wenn sich der Wert signifikant geändert hat
inline bool updateAndCheckU32(uint32_t* dest, uint32_t newVal,
                               uint32_t invalidSentinel, uint32_t delta) {
    if (dest == nullptr) return false;
    const bool changed = valueChangedSignificantU32(*dest, newVal, invalidSentinel, delta);
    *dest = newVal;
    return changed;
}

// updateAndCheckU16 – Wert schreiben UND auf signifikante Änderung prüfen.
//   Wie updateAndCheckU32, aber für uint16-Werte (Temperatur, Feuchte, Lux).
inline bool updateAndCheckU16(uint16_t* dest, uint16_t newVal,
                               uint16_t invalidSentinel, uint16_t delta) {
    if (dest == nullptr) return false;
    const bool changed = valueChangedSignificantU16(*dest, newVal, invalidSentinel, delta);
    *dest = newVal;
    return changed;
}

} // namespace SmartHome
