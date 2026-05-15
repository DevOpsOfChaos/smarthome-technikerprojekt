/**
 * =============================================================================
 * @modul     command_minimal
 * @beschreibung  Erzeugt validierte Steuerbefehle (MQTT-Commands) für die
 *                offiziellen Command-Endpunkte des Servers.
 *
 * @endpunkte
 *   - POST /api/phase1/net-erl/relay-1   → Relais schalten (net_erl-Geräte)
 *   - POST /api/phase1/cover/command     → Rolladen steuern (net_zrl-Geräte)
 *
 * @funktionen
 *   - buildNetErlRelay1Command  → Validierten relay_1-Schaltbefehl erzeugen
 *   - buildCoverCommand         → Validierten Rolladen-Befehl (open/close/stop/set_position)
 *   - buildRequestId            → Eindeutige Request-ID pro Befehl
 *
 * @nutzung   40_command_minimal.json (Flow), 41_cover_automation_detail.json (Tick)
 * @export    buildNetErlRelay1Command, buildCoverCommand
 * =============================================================================
 */

"use strict";

const crypto = require("crypto");
const { nowIso, isPlainObject } = require("./time_helpers");
const { isCoverDevice, inferBaseTypeFromDeviceId } = require("./capability_helpers");

// ===========================================================================
// MUSTER FÜR GERÄTEKLASSEN
// ===========================================================================

const NET_ERL_DEVICE_CLASS_PATTERN = /^net[-_]?erl$/i;

// ===========================================================================
// ERLAUBTE BEFEHLE
// ===========================================================================

const COVER_COMMANDS = new Set(["open", "close", "stop", "set_position"]);

// ===========================================================================
// PRIVATE HELFER
// ===========================================================================

/**
 * Normalisiert beliebige Eingabe zu einem einfachen Objekt.
 * JSON-Strings werden geparst, Arrays und Nicht-Objekte geben {} zurück.
 */
function normalizeInput(input) {
  if (isPlainObject(input)) {
    return input;
  }

  if (typeof input === "string") {
    try {
      const parsed = JSON.parse(input);
      return isPlainObject(parsed) ? parsed : {};
    } catch (_error) {
      return {};
    }
  }

  return {};
}

/**
 * Normalisiert eine Device-ID zu einem getrimmten String.
 */
function normalizeDeviceId(value) {
  return typeof value === "string" ? value.trim() : "";
}

/**
 * Normalisiert einen Relay-Wert auf boolean.
 * Akzeptiert booleans, "true"/"false", "1"/"0", "on"/"off".
 *
 * @returns {{ ok: boolean, value: boolean|null }}
 */
function normalizeRelayValue(value) {
  if (typeof value === "boolean") {
    return { ok: true, value };
  }

  if (typeof value === "string") {
    const normalized = value.trim().toLowerCase();
    if (["true", "1", "on"].includes(normalized)) {
      return { ok: true, value: true };
    }
    if (["false", "0", "off"].includes(normalized)) {
      return { ok: true, value: false };
    }
  }

  return { ok: false, value: null };
}

/**
 * Bereinigt eine Device-ID für die Verwendung in request_id-Strings.
 * Entfernt Sonderzeichen und normalisiert auf Kleinbuchstaben.
 * Leeres Ergebnis → Fallback "device".
 */
function sanitizeDeviceId(deviceId) {
  return deviceId
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "") || "device";
}

/**
 * Normalisiert einen Kommandotext zu Kleinbuchstaben, getrimmt.
 */
function normalizeCommand(value) {
  return typeof value === "string" ? value.trim().toLowerCase() : "";
}

/**
 * Normalisiert einen Wert auf eine ganze Zahl.
 * Akzeptiert numbers und numerische Strings.
 *
 * @returns {{ ok: boolean, value: number|null }}
 */
function normalizeInteger(value) {
  if (typeof value === "number" && Number.isInteger(value)) {
    return { ok: true, value };
  }

  if (typeof value === "string" && value.trim() !== "") {
    const parsed = Number(value);
    if (Number.isInteger(parsed)) {
      return { ok: true, value: parsed };
    }
  }

  return { ok: false, value: null };
}

// ===========================================================================
// REQUEST-ID-ERZEUGUNG
// ===========================================================================

/**
 * Erzeugt eine eindeutige Request-ID für einen Steuerbefehl.
 *
 * Format: cmd-<device>-<action>-<kompakterZeitstempel>-<zufallssuffix>
 * Beispiel: cmd-net-erl-01-relay1-on-20260515143000000-a1b2c3
 *
 * @param {string} deviceId  - Gerätekennung
 * @param {string} action    - Aktionsbezeichnung
 * @param {string} timestamp - ISO-Zeitstempel (Standard: jetzt)
 * @returns {string} Eindeutige Request-ID
 */
function buildRequestId(deviceId, action, timestamp = nowIso()) {
  const compactTimestamp = String(timestamp).replace(/[^0-9]/g, "");
  const suffix = crypto.randomBytes(3).toString("hex");
  return `cmd-${sanitizeDeviceId(deviceId)}-${sanitizeDeviceId(action)}-${compactTimestamp}-${suffix}`;
}

// ===========================================================================
// FEHLERHILFSFUNKTION
// ===========================================================================

function buildError(statusCode, error, message) {
  return {
    ok: false,
    statusCode,
    error,
    message
  };
}

// ===========================================================================
// BEFEHLSERZEUGUNG – NET_ERL (RELAIS)
// ===========================================================================

/**
 * Prüft, ob ein Gerät ein net_erl-Aktor ist (Relais-Schaltgerät).
 * Prüft device_class aus den Metadaten (primär) oder Device-ID-Präfix (Fallback).
 *
 * @param {object} runtime  - Laufzeitzustand
 * @param {string} deviceId - Gerätekennung
 * @returns {boolean}
 */
function isNetErlDevice(runtime, deviceId) {
  const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
  const deviceClass = device && device.meta && typeof device.meta.device_class === "string"
    ? device.meta.device_class
    : "";

  if (deviceClass) {
    return NET_ERL_DEVICE_CLASS_PATTERN.test(deviceClass);
  }

  const baseType = inferBaseTypeFromDeviceId(deviceId);
  return baseType === "net_erl";
}

/**
 * Erzeugt einen validierten Relay-1-Schaltbefehl für net_erl-Geräte.
 *
 * Validiert:
 *   1. device_id muss vorhanden sein
 *   2. Gerät muss ein net_erl-Aktor sein
 *   3. relay_1 muss true oder false sein
 *
 * @param {object} runtime   - Laufzeitzustand
 * @param {object} input     - Payload mit device_id und relay_1 (true/false)
 * @param {string} timestamp - Befehlszeitpunkt
 * @returns {{ ok: true, command, response } | { ok: false, statusCode, error, message }}
 */
function buildNetErlRelay1Command(runtime, input, timestamp = nowIso()) {
  const payload = normalizeInput(input);
  const deviceId = normalizeDeviceId(payload.device_id);
  if (!deviceId) {
    return buildError(400, "device_id_required", "device_id is required");
  }

  if (!isNetErlDevice(runtime, deviceId)) {
    return buildError(400, "unsupported_device", "only net_erl relay_1 commands are allowed");
  }

  const relayValue = Object.prototype.hasOwnProperty.call(payload, "relay_1")
    ? payload.relay_1
    : payload.value;
  const normalizedRelayValue = normalizeRelayValue(relayValue);
  if (!normalizedRelayValue.ok) {
    return buildError(400, "relay_1_required", "relay_1 must be true or false");
  }

  const requestId = buildRequestId(deviceId, `relay1-${normalizedRelayValue.value ? "on" : "off"}`, timestamp);
  const topic = `smarthome/device/${deviceId}/command`;
  const commandPayload = {
    device_id: deviceId,
    request_id: requestId,
    command: "set_relay",
    relay_1: normalizedRelayValue.value
  };

  return {
    ok: true,
    command: {
      topic,
      payload: commandPayload
    },
    response: {
      device_id: deviceId,
      request_id: requestId,
      command: "set_relay",
      relay_1: normalizedRelayValue.value,
      mqtt_topic: topic,
      accepted_at: timestamp
    }
  };
}

// ===========================================================================
// BEFEHLSERZEUGUNG – NET_ZRL (ROLLADEN)
// ===========================================================================

/**
 * Erzeugt einen validierten Rolladen-Steuerbefehl für net_zrl-Geräte.
 *
 * Validiert:
 *   1. device_id muss vorhanden sein
 *   2. Gerät muss ein Rolladen-Controller sein (isCoverDevice aus capability_helpers)
 *   3. Befehl muss open, close, stop oder set_position sein
 *   4. Bei set_position: position muss 0–100 sein
 *   5. Ohne Kalibrierung: nur Endlagen 0/100 erlaubt
 *
 * @param {object} runtime   - Laufzeitzustand
 * @param {object} input     - Payload mit device_id, command, ggf. position
 * @param {string} timestamp - Befehlszeitpunkt
 * @returns {{ ok: true, command, response } | { ok: false, statusCode, error, message }}
 */
function buildCoverCommand(runtime, input, timestamp = nowIso()) {
  const payload = normalizeInput(input);
  const deviceId = normalizeDeviceId(payload.device_id);
  if (!deviceId) {
    return buildError(400, "device_id_required", "device_id is required");
  }

  if (!isCoverDevice(runtime, deviceId)) {
    return buildError(400, "unsupported_device", "only cover commands are allowed");
  }

  const command = normalizeCommand(payload.command);
  if (!COVER_COMMANDS.has(command)) {
    return buildError(400, "invalid_command", "command must be open, close, stop or set_position");
  }

  const requestId = buildRequestId(deviceId, command, timestamp);
  const topic = `smarthome/device/${deviceId}/command`;
  const commandPayload = {
    device_id: deviceId,
    request_id: requestId,
    command
  };

  if (command === "set_position") {
    const normalizedPosition = normalizeInteger(payload.position);
    if (!normalizedPosition.ok || normalizedPosition.value < 0 || normalizedPosition.value > 100) {
      return buildError(400, "position_required", "position must be an integer from 0 to 100");
    }

    // Kalibrierungsstatus prüfen: ohne Kalibrierung nur Endlagen.
    const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
    const coverCalibratedHint = payload.cover_calibrated ?? payload.is_calibrated;
    const coverCalibrated = device && device.state
      ? (device.state.cover_calibrated ?? device.state.is_calibrated ?? coverCalibratedHint)
      : coverCalibratedHint;
    const coverIsCalibrated = coverCalibrated === true || coverCalibrated === 1;
    const coverIsExplicitlyUncalibrated = coverCalibrated === false || coverCalibrated === 0;

    if (coverIsExplicitlyUncalibrated && !coverIsCalibrated && ![0, 100].includes(normalizedPosition.value)) {
      return buildError(409, "not_calibrated", "set_position without calibration only allows 0 or 100");
    }

    commandPayload.position = normalizedPosition.value;
  }

  return {
    ok: true,
    command: {
      topic,
      payload: commandPayload
    },
    response: {
      device_id: deviceId,
      request_id: requestId,
      command,
      position: Object.prototype.hasOwnProperty.call(commandPayload, "position") ? commandPayload.position : undefined,
      mqtt_topic: topic,
      accepted_at: timestamp
    }
  };
}

module.exports = {
  buildCoverCommand,
  buildNetErlRelay1Command
};
