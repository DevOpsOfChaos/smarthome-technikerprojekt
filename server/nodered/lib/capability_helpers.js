/**
 * =============================================================================
 * @modul     capability_helpers
 * @beschreibung  Gerätefähigkeiten (Capabilities) normalisieren, ableiten und prüfen.
 *                Zentrale Definition für alle Gerätetypen – keine andere Datei
 *                soll eigene Capability- oder Gerätetyp-Prüfungen duplizieren.
 *
 * @funktionen
 *   - inferBaseTypeFromDeviceId  → Basistyp aus Device-ID ableiten (Fallback)
 *   - normalizeCapabilities      → Roh-Caps in sortierte, kanonische Liste umwandeln
 *   - deriveCapabilities         → Vollständige Capability-Liste eines Geräts berechnen
 *   - hasCapability              → Einzelne Fähigkeit prüfen
 *   - isCoverDevice              → Prüft, ob Gerät ein Rolladen-Controller ist (shared)
 *
 * @nutzung   device_store.js, dashboard_v1.js, command_minimal.js, cover_automation.js
 * @export    inferBaseTypeFromDeviceId, normalizeCapabilities, deriveCapabilities,
 *            hasCapability, isCoverDevice, ALWAYS_PRESENT_CAPABILITIES
 * =============================================================================
 */

"use strict";

// ===========================================================================
// ALIASE FÜR ALTERNATIVE SCHREIBWEISEN
// ===========================================================================
// Eingehende Werte werden auf den kanonischen Namen normalisiert.
// Beispiel: "relay" → "switchable", "temperature" → "temp"

const CAPABILITY_ALIASES = new Map([
  ["switch",          "switchable"],
  ["relay",           "switchable"],
  ["temperature",     "temp"],
  ["humidity",        "hum"],
  ["aqi",             "air_quality"],
  ["window_contact",  "window"],
  ["rain_sensor",     "rain"]
]);

// ===========================================================================
// IMMER VORHANDENE FÄHIGKEITEN
// ===========================================================================
// Diese Fähigkeiten hat jedes Gerät, unabhängig von seiner Klasse.
// online_state:   Der Server trackt online/offline für jedes Gerät.
// fault_state:    Jedes Gerät kann einen Fehlerzustand melden (fault-Flag).
// ack_tracking:   Jedes Gerät kann Befehlsbestätigungen (Acks) senden.

const ALWAYS_PRESENT_CAPABILITIES = ["online_state", "fault_state", "ack_tracking"];

// ===========================================================================
// BITMASKE FÜR NUMERISCHE CAPABILITY-FELDER
// ===========================================================================
// Gerätefirmware kann Fähigkeiten als Bitmaske im meta.caps-Feld senden.
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

// ===========================================================================
// MUSTER FÜR GERÄTEKLASSEN-ERKENNUNG
// ===========================================================================
// Kanonische Patterns für die Klassifizierung von Geräten anhand ID oder Klasse.

const NET_ERL_DEVICE_CLASS_PATTERN = /^net[-_]?erl$/i;
const NET_ZRL_DEVICE_CLASS_PATTERN = /^net[-_]?zrl$/i;

// ===========================================================================
// BASISTYP-ABLEITUNG
// ===========================================================================

/**
 * Leitet den Basistyp eines Geräts aus seiner Device-ID ab.
 * Wird nur als Fallback genutzt, wenn device_class aus den Metadaten fehlt.
 *
 * @param {string} deviceId - Gerätekennung (z. B. "net_erl_01", "master_gw1")
 * @returns {string} Normalisierter Basistyp (z. B. "net_erl", "master")
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

// ===========================================================================
// CAPABILITY-NORMALISIERUNG
// ===========================================================================

/**
 * Normalisiert einen Capability-Rohwert auf eine sortierte Liste kanonischer Namen.
 *
 * Akzeptiert folgende Eingabeformate:
 *   - Ganzzahl (Bitmaske, z. B. 0x0005)
 *   - String mit Bitmaske als Dezimalzahl (z. B. "5")
 *   - Komma-/Semikolon-/Leerzeichen-getrennter String (z. B. "temp,hum,motion")
 *   - Array von Strings
 *
 * Aliase werden automatisch aufgelöst (z. B. "relay" → "switchable").
 *
 * @param {number|string|string[]} rawCaps - Rohe Fähigkeitsangabe
 * @returns {string[]} Sortierte Liste kanonischer Fähigkeitsnamen
 */
function normalizeCapabilities(rawCaps) {
  if (typeof rawCaps === "number" && Number.isInteger(rawCaps)) {
    // Bitmaske: gesetzte Bits auf Capability-Namen abbilden.
    return CAPABILITY_BITS
      .filter(([bit]) => (rawCaps & bit) !== 0)
      .map(([, capability]) => capability)
      .sort();
  }

  const rawText = typeof rawCaps === "string" ? rawCaps.trim() : "";
  if (/^\d+$/.test(rawText)) {
    // String enthält reine Dezimalzahl → als Bitmaske interpretieren.
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
    // Alias auflösen, falls vorhanden; sonst Wert direkt übernehmen.
    normalized.add(CAPABILITY_ALIASES.get(entry) || entry);
  }

  return Array.from(normalized).sort();
}

/**
 * Leitet die vollständige Fähigkeitsliste eines Geräts ab.
 *
 * Die Liste setzt sich zusammen aus:
 *   1. Expliziten Caps aus der Bitmaske/String-Liste (normalisiert)
 *   2. Klassenbasierten Ergänzungen:
 *      - net_zrl (Rolladen-Controller) → erhält automatisch "cover"
 *      - bat_sen (Batteriegerät)        → erhält automatisch "battery"
 *   3. Immer vorhandenen Basis-Caps (online_state, fault_state, ack_tracking)
 *
 * @param {object} meta - Meta-Objekt mit device_class und caps
 * @returns {string[]} Sortierte Liste aller Fähigkeiten
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

  if (["window", "fenster", "door", "tuer", "tür"].includes(String(meta.contact_type || "").trim().toLowerCase())) {
    caps.add("window");
  }

  for (const capability of ALWAYS_PRESENT_CAPABILITIES) {
    caps.add(capability);
  }

  return Array.from(caps).sort();
}

/**
 * Prüft, ob ein Gerät eine bestimmte Fähigkeit besitzt.
 * Der Fähigkeitsname wird vor dem Vergleich alias-normalisiert.
 *
 * @param {object} meta       - Meta-Objekt mit device_class und caps
 * @param {string} capability - Zu prüfender Fähigkeitsname
 * @returns {boolean}
 */
function hasCapability(meta, capability) {
  const expected = String(capability || "").trim().toLowerCase();
  return deriveCapabilities(meta).includes(CAPABILITY_ALIASES.get(expected) || expected);
}

// ===========================================================================
// GERÄTETYP-PRÜFUNGEN (gemeinsam genutzt)
// ===========================================================================

/**
 * Prüft, ob ein Gerät ein Rolladen-Controller (net_zrl / Cover) ist.
 *
 * Prüfreihenfolge:
 *   1. control_mode === "cover" / "shutter" / "blind" in den Metadaten
 *   2. device_class entspricht net_zrl-Muster
 *   3. "cover" ist in der Fähigkeitsliste (caps) enthalten
 *   4. Fallback: Device-ID-Präfix prüfen
 *
 * Zentralisierte Funktion – command_minimal.js und cover_automation.js
 * nutzen beide diese Version statt eigener Implementierungen.
 *
 * @param {object} runtime  - Laufzeitzustand mit devices-Map
 * @param {string} deviceId - Gerätekennung
 * @returns {boolean}
 */
function isCoverDevice(runtime, deviceId) {
  const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
  if (!device) {
    return false;
  }

  const meta = device.meta || {};
  const controlMode = String(meta.control_mode || "").toLowerCase();
  const deviceClass = String(meta.device_class || "");
  const caps = Array.isArray(meta.caps)
    ? meta.caps.map((cap) => String(cap).toLowerCase())
    : [];

  // 1. Steuerungsmodus prüfen (expliziteste Quelle).
  if (controlMode === "cover" || controlMode.includes("shutter") || controlMode.includes("blind")) {
    return true;
  }

  // 2. Geräteklassen-Muster.
  if (NET_ZRL_DEVICE_CLASS_PATTERN.test(deviceClass)) {
    return true;
  }

  // 3. Fähigkeitsliste.
  if (caps.includes("cover")) {
    return true;
  }

  // 4. Fallback: Device-ID-Präfix.
  return NET_ZRL_DEVICE_CLASS_PATTERN.test(deviceId);
}

module.exports = {
  ALWAYS_PRESENT_CAPABILITIES,
  deriveCapabilities,
  hasCapability,
  inferBaseTypeFromDeviceId,
  isCoverDevice,
  normalizeCapabilities
};
