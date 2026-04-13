"use strict";

const capabilityHelpers = require("./capability_helpers");
const timeHelpers = require("./time_helpers");

const DEVICE_STATE_NORMALIZERS = {
  fault: normalizeBoolean,
  relay_1: normalizeBoolean,
  relay_2: normalizeBoolean,
  cover_mode: normalizeTextOrNull,
  cover_state: normalizeTextOrNull,
  cover_direction: normalizeTextOrNull,
  cover_position: normalizeNumber,
  cover_calibrated: normalizeBoolean,
  is_calibrated: normalizeBoolean,
  cover_moving: normalizeBoolean,
  cover_target: normalizeNumber,
  travel_time_ms: normalizeNumber,
  temp_01c: normalizeNumber,
  hum_01pct: normalizeNumber,
  lux: normalizeNumber,
  lux_01lx: normalizeNumber,
  pressure_pa: normalizeNumber,
  pressure_hpa: normalizeNumber,
  gas_ohm: normalizeNumber,
  aqi: normalizeNumber,
  tvoc_ppb: normalizeNumber,
  eco2_ppm: normalizeNumber,
  motion: normalizeBoolean,
  presence: normalizeBoolean,
  rain: normalizeBoolean,
  rain_raw: normalizeNumber,
  window_open: normalizeBoolean,
  contact_open: normalizeBoolean,
  battery_pct: normalizeNumber,
  battery_mv: normalizeNumber,
  button_flags: normalizeNumber,
  button_last_action: normalizeTextOrNull,
  button_last_action_at: normalizeTimestamp
};

const DEVICE_CONFIG_NORMALIZERS = {
  report_interval_s: normalizeNumber,
  lux_threshold_on: normalizeNumber,
  auto_off_delay_s: normalizeNumber,
  rain_threshold: normalizeNumber,
  auto_up_time: normalizeTextOrNull,
  auto_down_time: normalizeTextOrNull,
  auto_schedule_enabled: normalizeBoolean
};

const DEVICE_META_FIELDS = [
  "device_name",
  "device_class",
  "power_type",
  "fw_version",
  "control_mode",
  "config_profile",
  "reporting_mode",
  "sensor_mask",
  "input_mask",
  "mac_address",
  "meta_schema_version",
  "source"
];

function isPlainObject(value) {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

function normalizeTextOrNull(value) {
  if (value === undefined || value === null) {
    return null;
  }

  const text = String(value).trim();
  return text ? text : null;
}

function normalizeBoolean(value) {
  if (value === undefined || value === null || value === "") {
    return null;
  }

  return timeHelpers.coerceBoolean(value, false);
}

function normalizeNumber(value) {
  return timeHelpers.coerceNumber(value, null);
}

function normalizeTimestamp(value, fallback) {
  if (value === undefined || value === null || value === "") {
    return fallback || null;
  }

  return timeHelpers.coerceTimestamp(value, fallback || timeHelpers.nowIso());
}

function cloneObject(input) {
  return isPlainObject(input) ? { ...input } : {};
}

function createRuntimeState(now = timeHelpers.nowIso()) {
  return {
    devices: {},
    masters: {},
    initialized_at: now,
    schema_version: 1
  };
}

function ensureRuntime(runtimeState, now = timeHelpers.nowIso()) {
  if (!isPlainObject(runtimeState)) {
    return createRuntimeState(now);
  }

  if (!isPlainObject(runtimeState.devices)) {
    runtimeState.devices = {};
  }

  if (!isPlainObject(runtimeState.masters)) {
    runtimeState.masters = {};
  }

  if (!runtimeState.initialized_at) {
    runtimeState.initialized_at = now;
  }

  if (!runtimeState.schema_version) {
    runtimeState.schema_version = 1;
  }

  return runtimeState;
}

function createEmptyDevice(deviceId, now = timeHelpers.nowIso()) {
  return {
    identity: {
      device_id: deviceId,
      base_type: capabilityHelpers.inferBaseTypeFromDeviceId(deviceId),
      device_class: null,
      profile: null,
      device_name: deviceId,
      display_name: deviceId
    },
    meta: {},
    availability: {
      availability: "unknown",
      online: false,
      last_seen_at: null
    },
    state: {},
    config: {},
    last_event: null,
    last_ack: null,
    diagnostics: {
      auto_created: true,
      dropped_state_fields: [],
      last_handler: null,
      last_topic: null
    },
    created_at: now,
    updated_at: now,
    first_seen_at: now,
    last_seen_at: now,
    last_meta_at: null,
    last_availability_at: null,
    last_state_at: null,
    last_event_at: null,
    last_ack_at: null
  };
}

function createEmptyMaster(masterId, now = timeHelpers.nowIso()) {
  return {
    master_id: masterId,
    status: {
      master_id: masterId,
      online: false,
      wifi: false,
      mqtt: false,
      espnow: false,
      fw: null,
      last_seen_at: null,
      updated_at: now
    },
    last_event: null,
    diagnostics: {
      last_handler: null,
      last_topic: null
    },
    created_at: now,
    updated_at: now,
    first_seen_at: now,
    last_seen_at: now,
    last_status_at: null,
    last_event_at: null
  };
}

/*
 * autoCreate ist die fachliche Schalterstelle fuer den Ingest:
 * Bei echten MQTT-Nachrichten soll ein unbekanntes Geraet robust angelegt werden.
 * Bei lesenden oder pruefenden Pfaden darf stattdessen bewusst `null` zurueckkommen.
 */
function ensureDevice(runtimeState, deviceId, now = timeHelpers.nowIso(), options = {}) {
  const runtime = ensureRuntime(runtimeState, now);
  const normalizedId = normalizeTextOrNull(deviceId);
  if (!normalizedId) {
    return null;
  }

  const autoCreate = options.autoCreate !== false;
  if (!runtime.devices[normalizedId]) {
    if (!autoCreate) {
      return null;
    }
    runtime.devices[normalizedId] = createEmptyDevice(normalizedId, now);
  }

  return runtime.devices[normalizedId];
}

function ensureMaster(runtimeState, masterId, now = timeHelpers.nowIso()) {
  const runtime = ensureRuntime(runtimeState, now);
  const normalizedId = normalizeTextOrNull(masterId);
  if (!normalizedId) {
    return null;
  }

  if (!runtime.masters[normalizedId]) {
    runtime.masters[normalizedId] = createEmptyMaster(normalizedId, now);
  }

  return runtime.masters[normalizedId];
}

function touchDevice(device, receivedAt) {
  device.updated_at = receivedAt;
  device.last_seen_at = receivedAt;
}

function touchDeviceHandler(device, handlerName) {
  device.diagnostics.last_handler = handlerName;
  device.diagnostics.last_topic = handlerName;
}

function applyMeta(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const nextMeta = { ...cloneObject(device.meta), ...raw };

  DEVICE_META_FIELDS.forEach((fieldName) => {
    if (Object.prototype.hasOwnProperty.call(raw, fieldName)) {
      nextMeta[fieldName] = normalizeTextOrNull(raw[fieldName]);
    }
  });

  if (Object.prototype.hasOwnProperty.call(raw, "sim_case")) {
    nextMeta.sim_case = raw.sim_case;
  }

  nextMeta.device_id = device.identity.device_id;
  nextMeta.device_name = normalizeTextOrNull(nextMeta.device_name) || device.identity.device_name;
  nextMeta.device_class = normalizeTextOrNull(nextMeta.device_class);
  nextMeta.caps = capabilityHelpers.deriveCapabilities({
    device_class: nextMeta.device_class,
    caps: capabilityHelpers.normalizeCapabilities(nextMeta.caps)
  });

  device.meta = nextMeta;
  device.identity = {
    device_id: device.identity.device_id,
    base_type: device.identity.base_type,
    device_class: nextMeta.device_class || device.identity.device_class,
    profile: nextMeta.config_profile || device.identity.profile || null,
    device_name: nextMeta.device_name,
    display_name: nextMeta.device_name || device.identity.display_name
  };

  touchDevice(device, receivedAt);
  device.last_meta_at = receivedAt;
  touchDeviceHandler(device, "meta");
  return device;
}

function applyAvailability(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const previous = cloneObject(device.availability);
  const nextAvailability = { ...previous, ...raw };
  const availabilityLabel = normalizeTextOrNull(raw.availability);

  if (availabilityLabel) {
    nextAvailability.availability = availabilityLabel.toLowerCase();
  } else if (!normalizeTextOrNull(nextAvailability.availability)) {
    nextAvailability.availability = "unknown";
  }

  if (Object.prototype.hasOwnProperty.call(raw, "online")) {
    nextAvailability.online = timeHelpers.coerceBoolean(raw.online, false);
  } else if (nextAvailability.availability === "online") {
    nextAvailability.online = true;
  } else if (nextAvailability.availability === "offline") {
    nextAvailability.online = false;
  } else if (typeof previous.online === "boolean") {
    nextAvailability.online = previous.online;
  } else {
    nextAvailability.online = false;
  }

  nextAvailability.last_seen_at = normalizeTimestamp(raw.last_seen_at, receivedAt) || receivedAt;

  device.availability = nextAvailability;
  touchDevice(device, receivedAt);
  device.last_availability_at = receivedAt;
  touchDeviceHandler(device, "availability");
  return device;
}

function applyState(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const droppedStateFields = [];

  Object.entries(raw).forEach(([fieldName, fieldValue]) => {
    if (fieldName === "device_id") {
      return;
    }

    if (Object.prototype.hasOwnProperty.call(DEVICE_STATE_NORMALIZERS, fieldName)) {
      device.state[fieldName] = DEVICE_STATE_NORMALIZERS[fieldName](fieldValue);
      return;
    }

    if (Object.prototype.hasOwnProperty.call(DEVICE_CONFIG_NORMALIZERS, fieldName)) {
      device.config[fieldName] = DEVICE_CONFIG_NORMALIZERS[fieldName](fieldValue);
      return;
    }

    droppedStateFields.push(fieldName);
  });

  const coverCalibrated = device.state.cover_calibrated ?? device.state.is_calibrated;
  if (coverCalibrated !== undefined) {
    device.state.cover_calibrated = coverCalibrated;
    device.state.is_calibrated = coverCalibrated;
  }

  if (!isPlainObject(device.availability)) {
    device.availability = { availability: "unknown", online: false, last_seen_at: null };
  }
  device.availability.last_seen_at = receivedAt;

  touchDevice(device, receivedAt);
  device.last_state_at = receivedAt;
  device.diagnostics.dropped_state_fields = droppedStateFields;
  touchDeviceHandler(device, "state");
  return device;
}

function applyEvent(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const eventAt = normalizeTimestamp(raw.event_at || raw.occurred_at, receivedAt) || receivedAt;
  const eventTrigger = Object.prototype.hasOwnProperty.call(raw, "event_trigger") ? raw.event_trigger : raw.trigger;

  device.last_event = {
    ...raw,
    event_type: normalizeTextOrNull(raw.event_type),
    event_label: normalizeTextOrNull(raw.event_label || raw.event),
    event_trigger: normalizeTextOrNull(eventTrigger),
    param1: normalizeTextOrNull(raw.param1),
    param2: normalizeTextOrNull(raw.param2),
    event_at: eventAt
  };

  device.updated_at = receivedAt;
  device.last_event_at = eventAt;
  touchDeviceHandler(device, "event");
  return device;
}

function applyAck(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const ackAt = normalizeTimestamp(raw.ack_at || raw.occurred_at, receivedAt) || receivedAt;

  device.last_ack = {
    ...raw,
    request_id: normalizeTextOrNull(raw.request_id),
    channel: normalizeTextOrNull(raw.channel),
    status: normalizeTextOrNull(raw.status),
    status_code: normalizeTextOrNull(raw.status_code),
    ack_msg_type: normalizeTextOrNull(raw.ack_msg_type),
    ack_seq: normalizeTextOrNull(raw.ack_seq),
    source: normalizeTextOrNull(raw.source),
    ack_at: ackAt
  };

  device.updated_at = receivedAt;
  device.last_ack_at = ackAt;
  touchDeviceHandler(device, "ack");
  return device;
}

function applyMasterStatus(master, payload, receivedAt = timeHelpers.nowIso()) {
  if (!master) {
    return null;
  }

  const raw = cloneObject(payload);
  const lastSeenAt = normalizeTimestamp(raw.last_seen_at, receivedAt) || receivedAt;

  master.status = {
    ...cloneObject(master.status),
    ...raw,
    master_id: master.master_id,
    online: timeHelpers.coerceBoolean(raw.online, master.status.online || false),
    wifi: timeHelpers.coerceBoolean(raw.wifi, master.status.wifi || false),
    mqtt: timeHelpers.coerceBoolean(raw.mqtt, master.status.mqtt || false),
    espnow: timeHelpers.coerceBoolean(raw.espnow, master.status.espnow || false),
    fw: normalizeTextOrNull(raw.fw) || master.status.fw || null,
    last_seen_at: lastSeenAt,
    updated_at: receivedAt
  };

  master.updated_at = receivedAt;
  master.last_seen_at = lastSeenAt;
  master.last_status_at = receivedAt;
  master.diagnostics.last_handler = "status";
  master.diagnostics.last_topic = "status";
  return master;
}

function applyMasterEvent(master, payload, receivedAt = timeHelpers.nowIso()) {
  if (!master) {
    return null;
  }

  const raw = cloneObject(payload);
  const occurredAt = normalizeTimestamp(raw.occurred_at, receivedAt) || receivedAt;

  if (!isPlainObject(master.status)) {
    master.status = createEmptyMaster(master.master_id, receivedAt).status;
  }

  master.last_event = {
    ...raw,
    event: normalizeTextOrNull(raw.event),
    message: normalizeTextOrNull(raw.message),
    fw: normalizeTextOrNull(raw.fw),
    occurred_at: occurredAt
  };

  master.updated_at = receivedAt;
  master.last_event_at = occurredAt;
  master.diagnostics.last_handler = "event";
  master.diagnostics.last_topic = "event";
  return master;
}

function textOrNull(value) {
  return value === undefined || value === null ? null : String(value);
}

/*
 * Diese Row passt exakt zur Tabelle `devices`.
 * Laufzeit-interne Zusatzfelder wie diagnostics oder Zeitmarken pro Block bleiben bewusst draussen.
 */
function buildDeviceRow(device) {
  const identity = cloneObject(device && device.identity);
  const meta = cloneObject(device && device.meta);

  return {
    device_id: identity.device_id,
    device_name: identity.device_name || meta.device_name || identity.device_id,
    device_class: meta.device_class || identity.device_class || null,
    power_type: meta.power_type || null,
    fw_version: meta.fw_version || null,
    caps: capabilityHelpers.deriveCapabilities({
      device_class: meta.device_class,
      caps: meta.caps
    }),
    control_mode: meta.control_mode || null,
    config_profile: meta.config_profile || null,
    reporting_mode: meta.reporting_mode || null,
    sensor_mask: meta.sensor_mask || null,
    input_mask: meta.input_mask || null,
    mac_address: meta.mac_address || null,
    meta_schema_version: meta.meta_schema_version || null,
    created_at: device.created_at,
    updated_at: device.updated_at
  };
}

/*
 * Diese Row bildet nur die realen Spalten aus `device_state_latest` ab.
 * Das Laufzeitobjekt darf mehr wissen, aber SQLite bekommt hier nur den offiziellen Serverschnitt.
 */
function buildDeviceStateLatestRow(device) {
  const availability = cloneObject(device && device.availability);
  const state = cloneObject(device && device.state);
  const config = cloneObject(device && device.config);
  const event = cloneObject(device && device.last_event);
  const ack = cloneObject(device && device.last_ack);
  const coverCalibrated = state.cover_calibrated ?? state.is_calibrated;

  return {
    device_id: device.identity.device_id,
    availability: availability.availability || "unknown",
    online: Boolean(availability.online),
    last_seen_at: availability.last_seen_at || null,
    fault: Boolean(state.fault),
    relay_1: state.relay_1,
    relay_2: state.relay_2,
    cover_mode: state.cover_mode,
    cover_state: state.cover_state,
    cover_direction: state.cover_direction,
    cover_position: state.cover_position,
    cover_calibrated: coverCalibrated,
    is_calibrated: coverCalibrated,
    travel_time_ms: state.travel_time_ms,
    temp_01c: state.temp_01c,
    hum_01pct: state.hum_01pct,
    lux: state.lux,
    pressure_pa: state.pressure_pa,
    gas_ohm: state.gas_ohm,
    aqi: state.aqi,
    tvoc_ppb: state.tvoc_ppb,
    eco2_ppm: state.eco2_ppm,
    motion: state.motion,
    rain: state.rain,
    rain_raw: state.rain_raw,
    window_open: state.window_open,
    battery_pct: state.battery_pct,
    battery_mv: state.battery_mv,
    button_flags: state.button_flags,
    button_last_action: state.button_last_action,
    button_last_action_at: state.button_last_action_at,
    report_interval_s: config.report_interval_s,
    lux_threshold_on: config.lux_threshold_on,
    auto_off_delay_s: config.auto_off_delay_s,
    rain_threshold: config.rain_threshold,
    auto_up_time: config.auto_up_time,
    auto_down_time: config.auto_down_time,
    auto_schedule_enabled: config.auto_schedule_enabled,
    last_event_type: textOrNull(event.event_type),
    last_event_label: textOrNull(event.event_label),
    last_event_trigger: textOrNull(event.event_trigger),
    last_event_param1: textOrNull(event.param1),
    last_event_param2: textOrNull(event.param2),
    last_event_at: event.event_at || null,
    last_ack_request_id: textOrNull(ack.request_id),
    last_ack_channel: textOrNull(ack.channel),
    last_ack_status: textOrNull(ack.status),
    last_ack_status_code: textOrNull(ack.status_code),
    last_ack_msg_type: textOrNull(ack.ack_msg_type),
    last_ack_seq: textOrNull(ack.ack_seq),
    last_ack_source: textOrNull(ack.source),
    last_ack_at: ack.ack_at || null,
    updated_at: device.updated_at
  };
}

/*
 * Auch `master_status` bleibt flach. Event-Details gehoeren in `master_event_log`,
 * nicht als JSON-Blob in die Status-Tabelle.
 */
function buildMasterStatusRow(master) {
  const status = cloneObject(master && master.status);

  return {
    master_id: status.master_id || master.master_id,
    online: Boolean(status.online),
    wifi: Boolean(status.wifi),
    mqtt: Boolean(status.mqtt),
    espnow: Boolean(status.espnow),
    fw: status.fw || null,
    last_seen_at: status.last_seen_at || null,
    updated_at: status.updated_at || master.updated_at
  };
}

function buildDefaultUpdateColumns(columns, keyColumn) {
  return columns.filter((columnName) => {
    if (columnName === keyColumn) {
      return false;
    }

    // Einmal gesetzte Insert-Zeitstempel duerfen durch spaetere Meldungen nicht ueberschrieben werden.
    return columnName !== "created_at" && columnName !== "first_seen_at";
  });
}

function buildValueSqlList(columns, row, jsonColumns) {
  const jsonColumnSet = new Set(jsonColumns || []);
  return columns.map((columnName) => {
    const value = row[columnName];
    return jsonColumnSet.has(columnName)
      ? timeHelpers.toSqlJsonLiteral(value)
      : timeHelpers.toSqlLiteral(value);
  });
}

function buildUpsertSql(tableName, keyColumn, row, jsonColumns = [], updateColumns) {
  const columns = Object.keys(row);
  const updates = Array.isArray(updateColumns) && updateColumns.length
    ? updateColumns
    : buildDefaultUpdateColumns(columns, keyColumn);
  const valuesSql = buildValueSqlList(columns, row, jsonColumns);

  return [
    "INSERT INTO " + tableName + " (" + columns.join(", ") + ")",
    "VALUES (" + valuesSql.join(", ") + ")",
    "ON CONFLICT(" + keyColumn + ") DO UPDATE SET",
    updates.map((columnName) => columnName + " = excluded." + columnName).join(", "),
    ";"
  ].join(" ");
}

function buildDevicesUpsertSql(row) {
  return buildUpsertSql("devices", "device_id", row, ["caps"]);
}

function buildDeviceStateLatestUpsertSql(row) {
  return buildUpsertSql("device_state_latest", "device_id", row);
}

function buildMasterStatusUpsertSql(row) {
  return buildUpsertSql("master_status", "master_id", row);
}

module.exports = {
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  buildDeviceRow,
  buildDeviceStateLatestRow,
  buildDeviceStateLatestUpsertSql,
  buildDevicesUpsertSql,
  buildMasterStatusRow,
  buildMasterStatusUpsertSql,
  createRuntimeState,
  ensureDevice,
  ensureMaster,
  ensureRuntime
};
