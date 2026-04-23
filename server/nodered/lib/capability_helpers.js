"use strict";

// Aliase für alternative Schreibweisen von Fähigkeitsnamen.
// Eingehende Werte werden auf den kanonischen Namen normalisiert.
const CAPABILITY_ALIASES = new Map([
  ["switch",          "switchable"],
  ["relay",           "switchable"],
  ["temperature",     "temp"],
  ["humidity",        "hum"],
  ["aqi",             "air_quality"],
  ["window_contact",  "window"],
  ["rain_sensor",     "rain"]
]);

// Fähigkeiten, die jedes Gerät unabhängig von seiner Klasse besitzt.
const ALWAYS_PRESENT_CAPABILITIES = ["online_state", "fault_state", "ack_tracking"];

// Bitmaske zur Dekodierung numerischer Capability-Felder aus Gerätemetadaten.
// Jedes Bit entspricht einer bestimmten Fähigkeit.
const CAPABILITY_BITS = [
  [0x0001, "switchable"],
  [0x0002, "relay2"],
  [0x0004, "temp"],
  [0x0008, "hum"],
  [0x0010, "lux"],
  [0x0020, "air_quality"],
  [0x0040, "motion"],
  [0x0080, "window"],
  [0x0100, "rain"],
  [0x0200, "battery"],
  [0x0400, "button"],
  [0x0800, "multibutton"],
  [0x1000, "led_ring"],
  [0x2000, "cover"],
  [0x4000, "setup_portal"],
  [0x8000, "pressure"]
];

/*
 * Zweck: Leitet den Basistyp eines Geräts aus seiner Device-ID ab.
 *
 * Eingabe: deviceId als String (z. B. "net_erl_01", "master_gw1")
 * Rückgabe: Normalisierter Basistyp als String (z. B. "net_erl", "master"),
 *           oder der normalisierte Eingabestring bei unbekanntem Präfix.
 *
 * Hinweis: Wird nur als Fallback genutzt, wenn device_class aus Metadaten fehlt.
 */
function inferBaseTypeFromDeviceId(deviceId) {
  const normalized = String(deviceId || "").trim().toLowerCase();
  if (!normalized) {
    return "";
  }

  if (normalized.startsWith("master"))         return "master";
  if (/^net[-_]?erl/.test(normalized))         return "net_erl";
  if (/^net[-_]?zrl/.test(normalized))         return "net_zrl";
  if (/^net[-_]?sen/.test(normalized))         return "net_sen";
  if (/^bat[-_]?sen/.test(normalized))         return "bat_sen";

  return normalized;
}

/*
 * Zweck: Normalisiert einen Capability-Rohwert auf eine sortierte Liste kanonischer Namen.
 *
 * Eingabe: rawCaps kann sein:
 * - Ganzzahl (Bitmaske, z. B. 0x0005)
 * - String mit Bitmaske als Dezimalzahl
 * - komma-/semikolon-/leerzeichengetrennter String (z. B. "temp,hum,motion")
 * - Array von Strings
 *
 * Rückgabe: Sortiertes Array mit kanonischen Fähigkeitsnamen (Aliase bereits aufgelöst).
 */
function normalizeCapabilities(rawCaps) {
  if (typeof rawCaps === "number" && Number.isInteger(rawCaps)) {
    // Bitmaske auflösen: gesetzte Bits auf Capability-Namen abbilden
    return CAPABILITY_BITS
      .filter(([bit]) => (rawCaps & bit) !== 0)
      .map(([, capability]) => capability)
      .sort();
  }

  const rawText = typeof rawCaps === "string" ? rawCaps.trim() : "";
  if (/^\d+$/.test(rawText)) {
    return normalizeCapabilities(Number(rawText));
  }

  const values = Array.isArray(rawCaps)
    ? rawCaps
    : rawText
      ? rawCaps.split(/[;,\s]+/)
      : [];

  const normalized = new Set();
  for (const value of values) {
    const entry = String(value || "").trim().toLowerCase();
    if (!entry) {
      continue;
    }
    // Alias auflösen, falls vorhanden, sonst Wert direkt übernehmen
    normalized.add(CAPABILITY_ALIASES.get(entry) || entry);
  }

  return Array.from(normalized).sort();
}

/*
 * Zweck: Leitet die vollständige Fähigkeitsliste eines Geräts ab.
 *
 * Eingabe: meta-Objekt mit optionalen Feldern device_class und caps.
 *
 * Rückgabe: Sortiertes Array mit allen Fähigkeiten des Geräts,
 *           inklusive klassenbasierter Ergänzungen und immer vorhandener Basis-Caps.
 *
 * Besonderheit:
 * - net_zrl (Rolladen-Controller) erhält automatisch "cover"
 * - bat_sen (Batteriegerät) erhält automatisch "battery"
 * - Alle Geräte erhalten online_state, fault_state, ack_tracking
 */
function deriveCapabilities(meta = {}) {
  const caps = new Set(normalizeCapabilities(meta.caps));
  const deviceClass = String(meta.device_class || "").trim().toLowerCase().replace(/-/g, "_");

  if (deviceClass === "net_zrl") {
    caps.add("cover");
  }

  if (deviceClass === "bat_sen") {
    caps.add("battery");
  }

  for (const capability of ALWAYS_PRESENT_CAPABILITIES) {
    caps.add(capability);
  }

  return Array.from(caps).sort();
}

/*
 * Zweck: Prüft, ob ein Gerät eine bestimmte Fähigkeit besitzt.
 *
 * Eingabe: meta-Objekt und ein Fähigkeitsname (Alias wird aufgelöst).
 * Rückgabe: true, wenn die Fähigkeit vorhanden ist, sonst false.
 */
function hasCapability(meta, capability) {
  const expected = String(capability || "").trim().toLowerCase();
  return deriveCapabilities(meta).includes(CAPABILITY_ALIASES.get(expected) || expected);
}

module.exports = {
  ALWAYS_PRESENT_CAPABILITIES,
  deriveCapabilities,
  hasCapability,
  inferBaseTypeFromDeviceId,
  normalizeCapabilities
};
