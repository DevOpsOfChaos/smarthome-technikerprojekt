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

function normalizeCapabilities(rawCaps) {
  const values = Array.isArray(rawCaps)
    ? rawCaps
    : typeof rawCaps === "string"
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
  const deviceClass = String(meta.device_class || "").toUpperCase();

  if (deviceClass.includes("NET-ZRL")) {
    caps.add("cover");
  }

  if (deviceClass.includes("BAT-")) {
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
  normalizeCapabilities
};
