"use strict";

const { deriveCapabilities } = require("./capability_helpers");
const { coerceBoolean, coerceNumber, coerceTimestamp, nowIso } = require("./time_helpers");

const META_FIELDS = [
  "device_class",
  "power_type",
  "fw_version",
  "control_mode",
  "config_profile",
  "reporting_mode",
  "sensor_mask",
  "input_mask",
  "mac_address",
  "meta_schema_version"
];

const CONFIG_FIELDS = [
  "report_interval_s",
  "lux_threshold_on",
  "auto_off_delay_s",
  "rain_threshold",
  "auto_up_time",
  "auto_down_time",
  "auto_schedule_enabled"
];

const STATE_FIELD_TRANSFORMS = {
  fault: (value) => coerceBoolean(value, false),
  uptime_s: (value) => coerceNumber(value, null),
  relay_1: (value) => coerceBoolean(value, false),
  relay_2: (value) => coerceBoolean(value, false),
  cover_mode: (value) => value,
  cover_state: (value) => value,
  cover_direction: (value) => value,
  cover_position: (value) => coerceNumber(value, null),
  is_calibrated: (value) => coerceBoolean(value, false),
  travel_time_ms: (value) => coerceNumber(value, null),
  temp_01c: (value) => coerceNumber(value, null),
  hum_01pct: (value) => coerceNumber(value, null),
  lux: (value) => coerceNumber(value, null),
  pressure_pa: (value) => coerceNumber(value, null),
  gas_ohm: (value) => coerceNumber(value, null),
  aqi: (value) => coerceNumber(value, null),
  tvoc_ppb: (value) => coerceNumber(value, null),
  eco2_ppm: (value) => coerceNumber(value, null),
  motion: (value) => coerceBoolean(value, false),
  rain: (value) => coerceBoolean(value, false),
  rain_raw: (value) => coerceNumber(value, null),
  window_open: (value) => coerceBoolean(value, false),
  battery_pct: (value) => coerceNumber(value, null),
  battery_mv: (value) => coerceNumber(value, null),
  button_last_action: (value) => value,
  button_last_action_at: (value) => value
};

function createRuntimeState(timestamp = nowIso()) {
  return {
    schema_version: 1,
    initialized_at: timestamp,
    devices: {},
    master: {},
    diag: {
      booted_at: timestamp,
      last_boot_reason: "runtime"
    }
  };
}

function ensureRuntime(runtime, timestamp = nowIso()) {
  const nextRuntime = runtime && typeof runtime === "object"
    ? runtime
    : createRuntimeState(timestamp);

  nextRuntime.devices = nextRuntime.devices || {};
  nextRuntime.master = nextRuntime.master || {};
  nextRuntime.diag = nextRuntime.diag || {
    booted_at: timestamp,
    last_boot_reason: "runtime"
  };

  if (!nextRuntime.initialized_at) {
    nextRuntime.initialized_at = timestamp;
  }

  return nextRuntime;
}

function markRuntimeBoot(runtime, reason = "runtime", timestamp = nowIso()) {
  const nextRuntime = ensureRuntime(runtime, timestamp);
  nextRuntime.diag.booted_at = timestamp;
  nextRuntime.diag.last_boot_reason = reason;
  return nextRuntime;
}

function createEmptyDevice(deviceId, timestamp = nowIso()) {
  return {
    identity: {
      device_id: deviceId,
      device_name: deviceId
    },
    meta: {
      caps: []
    },
    availability: {
      availability: "unknown",
      online: false,
      last_seen_at: null
    },
    state: {
      fault: false
    },
    config: {},
    last_event: {},
    last_ack: {},
    diagnostics: {
      status_chip: "new",
      last_update_source: "boot",
      notes: ""
    },
    created_at: timestamp,
    updated_at: timestamp
  };
}

function createEmptyMaster(masterId, timestamp = nowIso()) {
  return {
    status: {
      master_id: masterId,
      online: false,
      wifi: false,
      mqtt: false,
      espnow: false,
      fw: null,
      last_seen_at: null,
      updated_at: timestamp
    },
    last_event: {},
    diagnostics: {
      last_update_source: "boot"
    }
  };
}

function ensureDevice(runtime, deviceId, timestamp = nowIso()) {
  runtime.devices = runtime.devices || {};
  if (!runtime.devices[deviceId]) {
    runtime.devices[deviceId] = createEmptyDevice(deviceId, timestamp);
  }
  return runtime.devices[deviceId];
}

function ensureMaster(runtime, masterId, timestamp = nowIso()) {
  runtime.master = runtime.master || {};
  if (!runtime.master[masterId]) {
    runtime.master[masterId] = createEmptyMaster(masterId, timestamp);
  }
  return runtime.master[masterId];
}

function deriveStatusChip(device) {
  if (!device.availability.online) {
    return "offline";
  }

  if (device.state.fault) {
    return "fault";
  }

  return "online";
}

function applyMeta(device, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  device.identity.device_id = nextPayload.device_id || device.identity.device_id;
  device.identity.device_name = nextPayload.device_name || device.identity.device_name;

  for (const field of META_FIELDS) {
    if (Object.prototype.hasOwnProperty.call(nextPayload, field) && nextPayload[field] !== undefined) {
      device.meta[field] = nextPayload[field];
    }
  }

  device.meta.caps = deriveCapabilities({
    ...device.meta,
    caps: nextPayload.caps || device.meta.caps
  });
  device.diagnostics.status_chip = deriveStatusChip(device);
  device.diagnostics.last_update_source = "meta";
  device.diagnostics.notes = device.meta.caps.join(", ");
  device.updated_at = timestamp;
  return device;
}

function applyAvailability(device, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  if (Object.prototype.hasOwnProperty.call(nextPayload, "availability")) {
    device.availability.availability = nextPayload.availability || device.availability.availability;
  }

  if (Object.prototype.hasOwnProperty.call(nextPayload, "online")) {
    device.availability.online = coerceBoolean(nextPayload.online, device.availability.online);
  } else if (nextPayload.availability) {
    device.availability.online = String(nextPayload.availability).toLowerCase() === "online";
  }

  device.availability.last_seen_at = timestamp;
  device.diagnostics.status_chip = deriveStatusChip(device);
  device.diagnostics.last_update_source = "availability";
  device.updated_at = timestamp;
  return device;
}

function applyState(device, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  for (const [field, transform] of Object.entries(STATE_FIELD_TRANSFORMS)) {
    if (Object.prototype.hasOwnProperty.call(nextPayload, field) && nextPayload[field] !== undefined) {
      device.state[field] = transform(nextPayload[field]);
    }
  }

  for (const field of CONFIG_FIELDS) {
    if (Object.prototype.hasOwnProperty.call(nextPayload, field) && nextPayload[field] !== undefined) {
      device.config[field] = field === "auto_schedule_enabled"
        ? coerceBoolean(nextPayload[field], false)
        : coerceNumber(nextPayload[field], nextPayload[field]);
    }
  }

  device.availability.online = true;
  device.availability.availability = nextPayload.availability || "online";
  device.availability.last_seen_at = timestamp;
  device.diagnostics.status_chip = deriveStatusChip(device);
  device.diagnostics.last_update_source = "state";
  device.updated_at = timestamp;
  return device;
}

function applyEvent(device, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  device.last_event = {
    event_type: nextPayload.event_type || nextPayload.event || null,
    event_label: nextPayload.event || nextPayload.event_label || nextPayload.event_type || null,
    event_trigger: nextPayload.trigger || null,
    param1: Object.prototype.hasOwnProperty.call(nextPayload, "param1") ? nextPayload.param1 : null,
    param2: Object.prototype.hasOwnProperty.call(nextPayload, "param2") ? nextPayload.param2 : null,
    event_at: coerceTimestamp(nextPayload.event_at, timestamp)
  };
  device.diagnostics.last_update_source = "event";
  device.updated_at = timestamp;
  return device;
}

function applyAck(device, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  device.last_ack = {
    request_id: nextPayload.request_id || null,
    channel: nextPayload.channel || null,
    status: nextPayload.status || null,
    status_code: Object.prototype.hasOwnProperty.call(nextPayload, "status_code") ? nextPayload.status_code : null,
    ack_msg_type: nextPayload.ack_msg_type || null,
    ack_seq: Object.prototype.hasOwnProperty.call(nextPayload, "ack_seq") ? nextPayload.ack_seq : null,
    ack_at: coerceTimestamp(nextPayload.ack_at, timestamp)
  };
  device.diagnostics.last_update_source = "ack";
  device.updated_at = timestamp;
  return device;
}

function applyMasterStatus(master, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  for (const field of ["online", "wifi", "mqtt", "espnow"]) {
    if (Object.prototype.hasOwnProperty.call(nextPayload, field)) {
      master.status[field] = coerceBoolean(nextPayload[field], master.status[field]);
    }
  }

  if (Object.prototype.hasOwnProperty.call(nextPayload, "fw")) {
    master.status.fw = nextPayload.fw;
  }

  master.status.last_seen_at = timestamp;
  master.status.updated_at = timestamp;
  master.diagnostics.last_update_source = "status";
  return master;
}

function applyMasterEvent(master, payload, timestamp = nowIso()) {
  const nextPayload = payload || {};
  master.last_event = {
    event: nextPayload.event || null,
    message: nextPayload.message || nextPayload.event || null,
    fw: nextPayload.fw || null,
    occurred_at: coerceTimestamp(nextPayload.occurred_at, timestamp)
  };
  master.diagnostics.last_update_source = "event";
  return master;
}

module.exports = {
  CONFIG_FIELDS,
  META_FIELDS,
  STATE_FIELD_TRANSFORMS,
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  createEmptyDevice,
  createEmptyMaster,
  createRuntimeState,
  ensureRuntime,
  ensureDevice,
  ensureMaster,
  markRuntimeBoot
};
