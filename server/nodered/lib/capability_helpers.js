"use strict";

const CAPABILITY_ALIASES = new Map([
  ["switch", "switchable"],
  ["relay", "switchable"],
  ["temperature", "temp"],
  ["humidity", "hum"],
  ["aqi", "air_quality"],
  ["window_contact", "window"],
  ["rain_sensor", "rain"]
]);

const ALWAYS_PRESENT_CAPABILITIES = ["online_state", "fault_state", "ack_tracking"];

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

function inferBaseTypeFromDeviceId(deviceId) {
  const normalized = String(deviceId || "").trim().toLowerCase();
  if (!normalized) {
    return "";
  }

  if (normalized.startsWith("master")) {
    return "master";
  }

  if (/^net[-_]?erl/.test(normalized)) {
    return "net_erl";
  }

  if (/^net[-_]?zrl/.test(normalized)) {
    return "net_zrl";
  }

  if (/^net[-_]?sen/.test(normalized)) {
    return "net_sen";
  }

  if (/^bat[-_]?sen/.test(normalized)) {
    return "bat_sen";
  }

  return normalized;
}

function normalizeCapabilities(rawCaps) {
  if (typeof rawCaps === "number" && Number.isInteger(rawCaps)) {
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
    normalized.add(CAPABILITY_ALIASES.get(entry) || entry);
  }

  return Array.from(normalized).sort();
}

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
