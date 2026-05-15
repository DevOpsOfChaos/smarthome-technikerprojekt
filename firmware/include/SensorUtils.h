// =============================================================================
// SensorUtils.h – Gemeinsame Sensor-Hilfsfunktionen (netzbetriebene Geräte)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/include/SensorUtils.h
// Teil des:   Baukastensystems Block 2 – Gemeinsame Services
//
// Verwendung: #include "SensorUtils.h" in net-Device-main.cpp
//             Alle Funktionen sind inline → Header-only.
//
// Enthält:
//   - Sensor-Recovery-Logik (automatische Wiederherstellung nach I2C-Ausfall)
//   - Stale-Daten-Erkennung (veraltete Messwerte nach Sensor-Ausfall)
//   - Gassensor-Warmup-Prüfung (BME680-Einlaufphase nach Boot)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#pragma once
#include <stdint.h>

namespace SmartHome {

// =============================================================================
// SENSOR-RECOVERY (Automatische Wiederherstellung)
// =============================================================================
// I2C-Sensoren können im Feld ausfallen (Bus-Hänger, transienter Fehler).
// Statt das Gerät dauerhaft blind zu lassen, wird periodisch ein Re-Init
// versucht. Diese Funktion steuert den Wiederholungs-Takt.

// recoveryIsDue – Prüft, ob ein erneuter Recovery-Versuch fällig ist.
//   lastAttemptMs: Zeitstempel des letzten Versuchs (0 = noch nie)
//   nowMs:         aktueller millis()-Wert
//   retryIntervalMs: Mindestabstand zwischen zwei Versuchen
//
//   lastAttemptMs == 0: noch nie versucht → sofort fällig (true)
//   Sonst: fällig wenn (nowMs - lastAttemptMs) >= retryIntervalMs
//
//   millis()-wrap-sicher durch unsigned-Arithmetik.
inline bool recoveryIsDue(unsigned long lastAttemptMs, unsigned long nowMs,
                           unsigned long retryIntervalMs) {
    return lastAttemptMs == 0UL || ((nowMs - lastAttemptMs) >= retryIntervalMs);
}

// =============================================================================
// STALE-DATA-ERKENNUNG (Veraltete Messwerte)
// =============================================================================
// Nach einem Sensor-Ausfall darf nicht ewig der letzte gültige Wert als
// "aktuell" gemeldet werden. Nach Ablauf des Stale-Timeout wird der Wert
// als veraltet markiert und UNGUELTIG gesendet.

// sensorValueStale – Prüft, ob ein Sensorwert als "veraltet" gilt.
//   sensorOk:       true = Sensor aktuell funktionsfähig
//   lastValidMs:    Zeitstempel der letzten gültigen Messung (0 = noch nie)
//   nowMs:          aktueller millis()-Wert
//   staleTimeoutMs: Timeout nach dem ein Wert als veraltet gilt
//
//   Rückgabe-Logik:
//     sensorOk == false          → true  (Sensor kaputt → sofort stale)
//     lastValidMs == 0           → false (Warmup – noch nie gemessen)
//     (nowMs - lastValidMs) > T  → true  (Timeout überschritten)
//     sonst                      → false (Wert noch frisch)
inline bool sensorValueStale(bool sensorOk, unsigned long lastValidMs,
                              unsigned long nowMs, unsigned long staleTimeoutMs) {
    if (!sensorOk) return true;
    if (lastValidMs == 0UL) return false;   // Noch kein gueltiger Wert
    return ((nowMs - lastValidMs) > staleTimeoutMs);
}

// =============================================================================
// GASSENSOR-WARMUP (BME680-Einlaufphase)
// =============================================================================
// Der BME680-Gassensor braucht nach dem Boot eine Einlaufphase (typisch 30min),
// bevor die Gas-Widerstandswerte stabil sind. Zusätzlich muss eine Mindestanzahl
// gültiger Messungen vorliegen.

// gasWarmupComplete – Prüft, ob die BME680-Einlaufphase abgeschlossen ist.
//   bootMs:     millis()-Wert zum Boot-Zeitpunkt
//   nowMs:      aktueller millis()-Wert
//   warmupMs:   erforderliche Warmup-Zeit (z.B. 1800000 = 30min)
//   validReads: Anzahl bisheriger gültiger Messungen
//   minReads:   mindestens erforderliche Anzahl (z.B. 20)
//
//   Beide Bedingungen müssen erfüllt sein (UND-Verknüpfung).
inline bool gasWarmupComplete(unsigned long bootMs, unsigned long nowMs,
                               unsigned long warmupMs,
                               unsigned int validReads, unsigned int minReads) {
    return ((nowMs - bootMs) >= warmupMs) && (validReads >= minReads);
}

} // namespace SmartHome
