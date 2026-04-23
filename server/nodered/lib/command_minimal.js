"use strict";

const crypto = require("crypto");
const { nowIso } = require("./time_helpers");

// Muster zur Geräteklassifizierung anhand ID oder device_class.
const NET_ERL_DEVICE_ID_PATTERN    = /^net_erl/i;
const NET_ERL_DEVICE_CLASS_PATTERN = /^net[-_]?erl$/i;
const NET_ZRL_DEVICE_ID_PATTERN    = /^net[-_]?zrl/i;
const NET_ZRL_DEVICE_CLASS_PATTERN = /^net[-_]?zrl$/i;

// Erlaubte Rolladen-Befehle.
const COVER_COMMANDS = new Set(["open", "close", "stop", "set_position"]);

function isPlainObject(value) {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

function normalizeInput(input) {
  if (isPlainObject(input)) {
    return input;
  }

  if (typeof input === "string") {
    try {
      const parsed = JSON.parse(input);
      return isPlainObject(parsed) ? parsed : {};
    } catch (error) {
      return {};
    }
  }

  return {};
}

function normalizeDeviceId(value) {
  return typeof value === "string" ? value.trim() : "";
}

/*
 * Zweck: Normalisiert einen Relay-Wert auf boolean.
 * Akzeptiert boolean, "true"/"false", "1"/"0", "on"/"off".
 * Rückgabe: { ok: true, value: boolean } oder { ok: false, value: null }.
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

// Bereinigt eine Device-ID für die Verwendung in request_id-Strings.
function sanitizeDeviceId(deviceId) {
  return deviceId
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "") || "device";
}

/*
 * Zweck: Erzeugt eine eindeutige Request-ID für einen Steuerbefehl.
 * Format: cmd-<device>-<action>-<zeitstempel>-<zufallssuffix>
 * Rückgabe: String, der pro Befehl eindeutig ist.
 */
function buildRequestId(deviceId, action, timestamp = nowIso()) {
  const compactTimestamp = String(timestamp).replace(/[^0-9]/g, "");
  const suffix = crypto.randomBytes(3).toString("hex");
  return `cmd-${sanitizeDeviceId(deviceId)}-${sanitizeDeviceId(action)}-${compactTimestamp}-${suffix}`;
}

/*
 * Zweck: Prüft, ob ein Gerät ein net_erl-Aktor ist (Relais-Schaltgerät).
 * Vorrang: device_class aus Metadaten, Fallback: Device-ID-Präfix.
 */
function isNetErlDevice(runtime, deviceId) {
  const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
  const deviceClass = device && device.meta && typeof device.meta.device_class === "string"
    ? device.meta.device_class
    : "";

  if (deviceClass) {
    return NET_ERL_DEVICE_CLASS_PATTERN.test(deviceClass);
  }

  return NET_ERL_DEVICE_ID_PATTERN.test(deviceId);
}

function normalizeCommand(value) {
  return typeof value === "string" ? value.trim().toLowerCase() : "";
}

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

/*
 * Zweck: Prüft, ob ein Gerät ein net_zrl-Rolladen-Controller ist.
 *
 * Reihenfolge der Prüfung:
 * 1. control_mode === "cover" aus Metadaten
 * 2. device_class Muster
 * 3. "cover" in Fähigkeitsliste
 * 4. Fallback auf Device-ID-Präfix
 */
function isCoverDevice(runtime, deviceId) {
  const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
  const deviceClass = device && device.meta && typeof device.meta.device_class === "string"
    ? device.meta.device_class
    : "";
  const controlMode = device && device.meta && typeof device.meta.control_mode === "string"
    ? device.meta.control_mode
    : "";
  const caps = device && device.meta && Array.isArray(device.meta.caps)
    ? device.meta.caps.map((cap) => String(cap).toLowerCase())
    : [];

  if (controlMode.toLowerCase() === "cover") {
    return true;
  }

  if (deviceClass) {
    return NET_ZRL_DEVICE_CLASS_PATTERN.test(deviceClass);
  }

  if (caps.includes("cover")) {
    return true;
  }

  return NET_ZRL_DEVICE_ID_PATTERN.test(deviceId);
}

function buildError(statusCode, error, message) {
  return {
    ok: false,
    statusCode,
    error,
    message
  };
}

/*
 * Zweck: Erzeugt einen validierten Relay-1-Schaltbefehl für net_erl-Geräte.
 *
 * Eingaben:
 * - runtime: aktueller Laufzeitzustand (für Geräteklassen-Prüfung)
 * - input: Payload mit device_id und relay_1 (true/false)
 * - timestamp: Befehlszeitpunkt (Standard: jetzt)
 *
 * Rückgabe: { ok, command: { topic, payload }, response } bei Erfolg,
 *           { ok: false, statusCode, error, message } bei Fehler.
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

/*
 * Zweck: Erzeugt einen validierten Rolladen-Steuerbefehl für net_zrl-Geräte.
 *
 * Eingaben:
 * - runtime: aktueller Laufzeitzustand (für Geräteklassen- und Kalibrierstatus-Prüfung)
 * - input: Payload mit device_id, command und ggf. position (0–100)
 * - timestamp: Befehlszeitpunkt (Standard: jetzt)
 *
 * Besonderheit:
 * - set_position ohne Kalibrierung erlaubt nur Endlagen (0 oder 100).
 *   Zwischenwerte wären technisch geraten und werden abgelehnt.
 *
 * Rückgabe: { ok, command: { topic, payload }, response } bei Erfolg,
 *           { ok: false, statusCode, error, message } bei Fehler.
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

    const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
    const coverCalibratedHint = payload.cover_calibrated ?? payload.is_calibrated;
    const coverCalibrated = device && device.state
      ? (device.state.cover_calibrated ?? device.state.is_calibrated ?? coverCalibratedHint)
      : coverCalibratedHint;
    const coverIsCalibrated = coverCalibrated === true || coverCalibrated === 1;
    const coverIsExplicitlyUncalibrated = coverCalibrated === false || coverCalibrated === 0;

    // Ohne Kalibrierung bleiben nur die Endlagen belastbar. Zwischenwerte würden hier geraten.
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
