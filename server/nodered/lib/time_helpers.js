/**
 * =============================================================================
 * @modul     time_helpers
 * @beschreibung  Zentrale Hilfsfunktionen für Zeitstempel, Typkonvertierung
 *                und Objektprüfung. Alle Server-Module nutzen diese Funktionen
 *                als gemeinsame Basis, um doppelten Code zu vermeiden.
 *
 * @export    nowIso, coerceTimestamp, coerceBoolean, coerceNumber, isPlainObject
 * =============================================================================
 */

"use strict";

// ===========================================================================
// KERN-ZEITFUNKTIONEN
// ===========================================================================

/**
 * Gibt den aktuellen Zeitpunkt als ISO-8601-String zurück.
 * Beispiel: "2026-05-15T14:30:00.000Z"
 *
 * @returns {string} ISO-Zeitstempel
 */
function nowIso() {
  return new Date().toISOString();
}

/**
 * Normalisiert einen beliebigen Zeitwert auf einen ISO-8601-String.
 * Akzeptiert Date-Objekte, Millisekunden-Zahlen, ISO-Strings und leere Werte.
 *
 * @param {*}      value    - Rohwert (String, Date, Zahl oder leer)
 * @param {string} fallback - Standardrückgabe bei ungültigem Wert (Standard: jetzt)
 * @returns {string} Gültiger ISO-String oder Fallback
 */
function coerceTimestamp(value, fallback = nowIso()) {
  if (!value) {
    return fallback;
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? fallback : parsed.toISOString();
}

// ===========================================================================
// TYP-KONVERTIERUNGEN
// ===========================================================================

/**
 * Normalisiert einen Wert auf boolean.
 * Versteht folgende Wahrheitswerte:
 *   true-Werte:  "1", "true", "yes", "on", "online", "open"
 *   false-Werte: "0", "false", "no", "off", "offline", "closed"
 * Bei Zahlen: 0 → false, alles andere → true.
 *
 * @param {*}       value    - Rohwert
 * @param {boolean} fallback - Standardrückgabe bei unbekanntem Wert
 * @returns {boolean}
 */
function coerceBoolean(value, fallback = false) {
  if (typeof value === "boolean") {
    return value;
  }

  if (typeof value === "number") {
    return value !== 0;
  }

  const normalized = String(value || "").trim().toLowerCase();
  if (["1", "true", "yes", "on", "online", "open"].includes(normalized)) {
    return true;
  }

  if (["0", "false", "no", "off", "offline", "closed"].includes(normalized)) {
    return false;
  }

  return fallback;
}

/**
 * Normalisiert einen Wert auf eine endliche Zahl.
 * Nicht-endliche Werte (NaN, Infinity, leer, null) geben den Fallback zurück.
 *
 * @param {*}      value    - Rohwert
 * @param {number} fallback - Standardrückgabe (Standard: null)
 * @returns {number|null}
 */
function coerceNumber(value, fallback = null) {
  if (value === null || value === undefined || value === "") {
    return fallback;
  }

  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

// ===========================================================================
// OBJEKT-PRÜFUNG (gemeinsam genutzt, um Duplikate zu vermeiden)
// ===========================================================================

/**
 * Prüft, ob ein Wert ein einfaches Objekt (kein Array, kein null) ist.
 * Zentrale Definition – keine andere Datei soll diese Logik duplizieren.
 *
 * @param {*} value - Zu prüfender Wert
 * @returns {boolean}
 */
function isPlainObject(value) {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

module.exports = {
  coerceBoolean,
  coerceNumber,
  coerceTimestamp,
  isPlainObject,
  nowIso
};
