"use strict";

function sqlLiteral(value) {
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
    return sqlLiteral(JSON.stringify(value));
  }

  return `'${String(value).replace(/'/g, "''")}'`;
}

function textOrNull(value) {
  return value === undefined || value === null ? null : String(value);
}

function buildInsert(tableName, values) {
  const entries = Object.entries(values);
  const columns = entries.map(([column]) => column).join(", ");
  const rowValues = entries.map(([, value]) => sqlLiteral(value)).join(", ");
  return `INSERT INTO ${tableName} (${columns}) VALUES (${rowValues})`;
}

function buildUpsert(tableName, values, conflictColumn, updateColumns) {
  const updates = updateColumns
    .map((column) => `${column} = excluded.${column}`)
    .join(", ");

  return `${buildInsert(tableName, values)} ON CONFLICT(${conflictColumn}) DO UPDATE SET ${updates}`;
}

function wrapBatch(statements) {
  const filtered = statements.filter(Boolean);
  if (!filtered.length) {
    return null;
  }

  return ["BEGIN IMMEDIATE", ...filtered, "COMMIT"]
    .map((statement) => `${statement};`)
    .join("\n");
}

function buildDeviceUpsert(device) {
  const values = {
    device_id: device.identity.device_id,
    device_name: device.identity.device_name,
    device_class: device.meta.device_class || null,
    power_type: device.meta.power_type || null,
    fw_version: device.meta.fw_version || null,
    caps: JSON.stringify(device.meta.caps || []),
    control_mode: device.meta.control_mode || null,
    config_profile: device.meta.config_profile || null,
    reporting_mode: device.meta.reporting_mode || null,
    sensor_mask: device.meta.sensor_mask || null,
    input_mask: device.meta.input_mask || null,
    mac_address: device.meta.mac_address || null,
    meta_schema_version: device.meta.meta_schema_version || null,
    created_at: device.created_at,
    updated_at: device.updated_at
  };

  return buildUpsert(
    "devices",
    values,
    "device_id",
    [
      "device_name",
      "device_class",
      "power_type",
      "fw_version",
      "caps",
      "control_mode",
      "config_profile",
      "reporting_mode",
      "sensor_mask",
      "input_mask",
      "mac_address",
      "meta_schema_version",
      "updated_at"
    ]
  );
}

function buildDeviceStateLatestUpsert(device) {
  const event = device.last_event || {};
  const ack = device.last_ack || {};
  const values = {
    device_id: device.identity.device_id,
    availability: device.availability.availability || "unknown",
    online: Boolean(device.availability.online),
    last_seen_at: device.availability.last_seen_at || null,
    fault: Boolean(device.state.fault),
    relay_1: device.state.relay_1,
    relay_2: device.state.relay_2,
    cover_mode: device.state.cover_mode,
    cover_state: device.state.cover_state,
    cover_position: device.state.cover_position,
    is_calibrated: device.state.is_calibrated,
    travel_time_ms: device.state.travel_time_ms,
    temp_01c: device.state.temp_01c,
    hum_01pct: device.state.hum_01pct,
    lux: device.state.lux,
    pressure_pa: device.state.pressure_pa,
    gas_ohm: device.state.gas_ohm,
    aqi: device.state.aqi,
    tvoc_ppb: device.state.tvoc_ppb,
    eco2_ppm: device.state.eco2_ppm,
    motion: device.state.motion,
    rain: device.state.rain,
    rain_raw: device.state.rain_raw,
    window_open: device.state.window_open,
    battery_pct: device.state.battery_pct,
    battery_mv: device.state.battery_mv,
    button_last_action: device.state.button_last_action,
    button_last_action_at: device.state.button_last_action_at,
    report_interval_s: device.config.report_interval_s,
    lux_threshold_on: device.config.lux_threshold_on,
    auto_off_delay_s: device.config.auto_off_delay_s,
    rain_threshold: device.config.rain_threshold,
    auto_up_time: device.config.auto_up_time,
    auto_down_time: device.config.auto_down_time,
    auto_schedule_enabled: device.config.auto_schedule_enabled,
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
    last_ack_at: ack.ack_at || null,
    updated_at: device.updated_at
  };

  const updateColumns = Object.keys(values).filter((column) => column !== "device_id");
  return buildUpsert("device_state_latest", values, "device_id", updateColumns);
}

function buildDeviceEventInsert(deviceId, event) {
  return buildInsert("device_event_log", {
    device_id: deviceId,
    event_type: textOrNull(event.event_type),
    event_label: textOrNull(event.event_label),
    event_trigger: textOrNull(event.event_trigger),
    param1: textOrNull(event.param1),
    param2: textOrNull(event.param2),
    occurred_at: event.event_at
  });
}

function buildDeviceAckInsert(deviceId, ack) {
  return buildInsert("device_ack_log", {
    device_id: deviceId,
    request_id: textOrNull(ack.request_id),
    channel: textOrNull(ack.channel),
    status: textOrNull(ack.status),
    status_code: textOrNull(ack.status_code),
    ack_msg_type: textOrNull(ack.ack_msg_type),
    ack_seq: textOrNull(ack.ack_seq),
    occurred_at: ack.ack_at
  });
}

function buildMasterStatusUpsert(master) {
  const values = {
    master_id: master.status.master_id,
    online: Boolean(master.status.online),
    wifi: Boolean(master.status.wifi),
    mqtt: Boolean(master.status.mqtt),
    espnow: Boolean(master.status.espnow),
    fw: master.status.fw || null,
    last_seen_at: master.status.last_seen_at || null,
    updated_at: master.status.updated_at
  };

  return buildUpsert(
    "master_status",
    values,
    "master_id",
    ["online", "wifi", "mqtt", "espnow", "fw", "last_seen_at", "updated_at"]
  );
}

function buildMasterEventInsert(masterId, event) {
  return buildInsert("master_event_log", {
    master_id: masterId,
    event: textOrNull(event.event),
    message: textOrNull(event.message),
    fw: textOrNull(event.fw),
    occurred_at: event.occurred_at
  });
}

function buildDeviceBatch(topicType, device) {
  const statements = [
    buildDeviceUpsert(device),
    buildDeviceStateLatestUpsert(device)
  ];

  if (topicType === "event" && device.last_event && device.last_event.event_at) {
    statements.push(buildDeviceEventInsert(device.identity.device_id, device.last_event));
  }

  if (topicType === "ack" && device.last_ack && device.last_ack.ack_at) {
    statements.push(buildDeviceAckInsert(device.identity.device_id, device.last_ack));
  }

  return wrapBatch(statements);
}

function buildMasterBatch(topicType, master) {
  const statements = [];

  if (topicType === "status" && master.status && master.status.master_id) {
    statements.push(buildMasterStatusUpsert(master));
  }

  if (topicType === "event" && master.status && master.status.master_id && master.last_event && master.last_event.occurred_at) {
    statements.push(buildMasterEventInsert(master.status.master_id, master.last_event));
  }

  return wrapBatch(statements);
}

function buildSqliteBatch(routed, payload) {
  if (!routed || typeof routed !== "object" || !payload || typeof payload !== "object") {
    return null;
  }

  if (routed.scope === "device" && payload.device) {
    return buildDeviceBatch(routed.topic_type, payload.device);
  }

  if (routed.scope === "master" && payload.master) {
    return buildMasterBatch(routed.topic_type, payload.master);
  }

  return null;
}

module.exports = {
  buildSqliteBatch,
  sqlLiteral
};
