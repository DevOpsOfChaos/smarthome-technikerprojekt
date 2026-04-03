"use strict";

function nowIso() {
  return new Date().toISOString();
}

function coerceTimestamp(value, fallback = nowIso()) {
  if (!value) {
    return fallback;
  }

  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? fallback : parsed.toISOString();
}

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
