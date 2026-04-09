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

function escapeSqlText(value) {
  return String(value).replace(/'/g, "''");
}

function toSqlLiteral(value) {
  if (value === undefined || value === null) {
    return "NULL";
  }

  if (typeof value === "boolean") {
    return value ? "1" : "0";
  }

  if (typeof value === "number") {
    return Number.isFinite(value) ? String(value) : "NULL";
  }

  if (Array.isArray(value) || typeof value === "object") {
    return toSqlLiteral(JSON.stringify(value));
  }

  return "'" + escapeSqlText(value) + "'";
}

function toSqlJsonLiteral(value) {
  if (value === undefined || value === null) {
    return "NULL";
  }

  return toSqlLiteral(JSON.stringify(value));
}

module.exports = {
  coerceBoolean,
  coerceNumber,
  coerceTimestamp,
  nowIso,
  toSqlJsonLiteral,
  toSqlLiteral
};
