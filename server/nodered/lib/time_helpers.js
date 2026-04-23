"use strict";

/*
 * Zweck: Gibt den aktuellen Zeitpunkt als ISO-8601-String zurück.
 * Rückgabe: z. B. "2026-04-23T07:00:00.000Z"
 */
function nowIso() {
  return new Date().toISOString();
}

/*
 * Zweck: Normalisiert einen beliebigen Zeitwert auf einen ISO-8601-String.
 *
 * Eingabe: value (String, Date, Zahl oder leer), fallback (Standardwert)
 * Rückgabe: Gültiger ISO-String, oder fallback bei ungültigem Wert.
 */
function coerceTimestamp(value, fallback = nowIso()) {
  if (!value) {
    return fallback;
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? fallback : parsed.toISOString();
}

/*
 * Zweck: Normalisiert einen Wert auf boolean.
 *
 * Versteht: true/false, 0/1, "true"/"false", "on"/"off", "online"/"offline", "open"/"closed".
 * Rückgabe: true, false, oder fallback bei unbekanntem Wert.
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

/*
 * Zweck: Normalisiert einen Wert auf eine endliche Zahl.
 *
 * Rückgabe: Zahl, oder fallback bei nicht-finitem Wert (null, NaN, Infinity, leer).
 */
function coerceNumber(value, fallback = null) {
  if (value === null || value === undefined || value === "") {
    return fallback;
  }

  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

module.exports = {
  coerceBoolean,
  coerceNumber,
  coerceTimestamp,
  nowIso
};
