"use strict";

const crypto = require("crypto");
const { nowIso } = require("./time_helpers");

const NET_ERL_DEVICE_ID_PATTERN = /^net_erl/i;
const NET_ERL_DEVICE_CLASS_PATTERN = /^net[-_]?erl$/i;

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

function sanitizeDeviceId(deviceId) {
  return deviceId
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "") || "device";
}

function buildRequestId(deviceId, relayState, timestamp = nowIso()) {
  const direction = relayState ? "on" : "off";
  const compactTimestamp = String(timestamp).replace(/[^0-9]/g, "");
  const suffix = crypto.randomBytes(3).toString("hex");
  return `cmd-${sanitizeDeviceId(deviceId)}-relay1-${direction}-${compactTimestamp}-${suffix}`;
}

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

function buildError(statusCode, error, message) {
  return {
    ok: false,
    statusCode,
    error,
    message
  };
}

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

  const requestId = buildRequestId(deviceId, normalizedRelayValue.value, timestamp);
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

module.exports = {
  buildNetErlRelay1Command
};
