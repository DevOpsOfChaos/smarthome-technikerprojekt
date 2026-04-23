"use strict";

/*
 * Zweck: Wandelt einen JavaScript-Wert in ein SQL-Literal um.
 * Unterstützte Typen: null/undefined → NULL, boolean → 0/1,
 * Zahl → Zahlenliteral, Objekt/Array → JSON-String-Literal, String → escaped String.
 */
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

/*
 * Zweck: Erzeugt ein einfaches INSERT-Statement für eine Tabelle.
 * Eingabe: Tabellenname und Werte-Objekt (Spaltenname → Wert).
 * Rückgabe: SQL-String ohne abschließendes Semikolon.
 */
function buildInsert(tableName, values) {
  const entries = Object.entries(values);
  const columns = entries.map(([column]) => column).join(", ");
  const rowValues = entries.map(([, value]) => sqlLiteral(value)).join(", ");
  return `INSERT INTO ${tableName} (${columns}) VALUES (${rowValues})`;
}

/*
 * Zweck: Erzeugt ein INSERT … ON CONFLICT … DO UPDATE SET (Upsert) Statement.
 * Eingabe: Tabellenname, Werte-Objekt, Konflikt-Spalte, Update-Spalten.
 * Rückgabe: SQL-String ohne abschließendes Semikolon.
 */
function buildUpsert(tableName, values, conflictColumn, updateColumns) {
  const updates = updateColumns
    .map((column) => `${column} = excluded.${column}`)
    .join(", ");

  return `${buildInsert(tableName, values)} ON CONFLICT(${conflictColumn}) DO UPDATE SET ${updates}`;
}

/*
 * Zweck: Verpackt mehrere SQL-Statements in eine atomare Transaktion.
 * Leere Statements werden gefiltert. Gibt null zurück, wenn nichts zu tun ist.
 * Rückgabe: BEGIN IMMEDIATE … COMMIT als einzelner String (für den sqlite-Node).
 */
function wrapBatch(statements) {
  const filtered = statements.filter(Boolean);
  if (!filtered.length) {
    return null;
  }

  return ["BEGIN IMMEDIATE", ...filtered, "COMMIT"]
    .map((statement) => `${statement};`)
    .join("\n");
}

/*
 * Zweck: Erzeugt das Upsert-Statement für die `devices`-Tabelle.
 * created_at und first_seen_at werden beim Update nicht überschrieben.
 */
function buildDeviceUpsert(device) {
  const values = {
    device_id:           device.identity.device_id,
    device_name:         device.identity.device_name,
    device_class:        device.meta.device_class || null,
    power_type:          device.meta.power_type || null,
    fw_version:          device.meta.fw_version || null,
    caps:                JSON.stringify(device.meta.caps || []),
    control_mode:        device.meta.control_mode || null,
    config_profile:      device.meta.config_profile || null,
    reporting_mode:      device.meta.reporting_mode || null,
    sensor_mask:         device.meta.sensor_mask || null,
    input_mask:          device.meta.input_mask || null,
    mac_address:         device.meta.mac_address || null,
    meta_schema_version: device.meta.meta_schema_version || null,
    created_at:          device.created_at,
    updated_at:          device.updated_at
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

/*
 * Zweck: Erzeugt das Upsert-Statement für die `device_state_latest`-Tabelle.
 * Schreibt den vollständigen aktuellen Snapshot eines Geräts.
 * Alle Spalten außer device_id werden bei jedem Upsert aktualisiert.
 */
function buildDeviceStateLatestUpsert(device) {
  const event = device.last_event || {};
  const ack = device.last_ack || {};
  // Beide Kalibrierungsfelder synchron halten (Firmware-Kompatibilität)
  const coverCalibrated = device.state.cover_calibrated ?? device.state.is_calibrated;

  const values = {
    device_id:             device.identity.device_id,
    availability:          device.availability.availability || "unknown",
    online:                Boolean(device.availability.online),
    last_seen_at:          device.availability.last_seen_at || null,
    fault:                 Boolean(device.state.fault),
    relay_1:               device.state.relay_1,
    relay_2:               device.state.relay_2,
    cover_mode:            device.state.cover_mode,
    cover_state:           device.state.cover_state,
    cover_direction:       device.state.cover_direction,
    cover_position:        device.state.cover_position,
    cover_calibrated:      coverCalibrated,
    is_calibrated:         coverCalibrated,
    travel_time_ms:        device.state.travel_time_ms,
    temp_01c:              device.state.temp_01c,
    hum_01pct:             device.state.hum_01pct,
    lux:                   device.state.lux,
    pressure_pa:           device.state.pressure_pa,
    gas_ohm:               device.state.gas_ohm,
    aqi:                   device.state.aqi,
    tvoc_ppb:              device.state.tvoc_ppb,
    eco2_ppm:              device.state.eco2_ppm,
    motion:                device.state.motion,
    rain:                  device.state.rain,
    rain_raw:              device.state.rain_raw,
    window_open:           device.state.window_open,
    battery_pct:           device.state.battery_pct,
    battery_mv:            device.state.battery_mv,
    button_flags:          device.state.button_flags,
    button_last_action:    device.state.button_last_action,
    button_last_action_at: device.state.button_last_action_at,
    report_interval_s:     device.config.report_interval_s,
    lux_threshold_on:      device.config.lux_threshold_on,
    auto_off_delay_s:      device.config.auto_off_delay_s,
    rain_threshold:        device.config.rain_threshold,
    auto_up_time:          device.config.auto_up_time,
    auto_down_time:        device.config.auto_down_time,
    auto_schedule_enabled: device.config.auto_schedule_enabled,
    last_event_type:       textOrNull(event.event_type),
    last_event_label:      textOrNull(event.event_label),
    last_event_trigger:    textOrNull(event.event_trigger),
    last_event_param1:     textOrNull(event.param1),
    last_event_param2:     textOrNull(event.param2),
    last_event_at:         event.event_at || null,
    last_ack_request_id:   textOrNull(ack.request_id),
    last_ack_channel:      textOrNull(ack.channel),
    last_ack_status:       textOrNull(ack.status),
    last_ack_status_code:  textOrNull(ack.status_code),
    last_ack_msg_type:     textOrNull(ack.ack_msg_type),
    last_ack_seq:          textOrNull(ack.ack_seq),
    last_ack_source:       textOrNull(ack.source),
    last_ack_at:           ack.ack_at || null,
    updated_at:            device.updated_at
  };

  const updateColumns = Object.keys(values).filter((column) => column !== "device_id");
  return buildUpsert("device_state_latest", values, "device_id", updateColumns);
}

/*
 * Zweck: Erzeugt das Upsert-Statement für die `master_status`-Tabelle.
 * Schreibt den aktuellen Snapshot eines Master-Geräts (kein Verlauf).
 */
function buildMasterStatusUpsert(master) {
  const values = {
    master_id:    master.status.master_id,
    online:       Boolean(master.status.online),
    wifi:         Boolean(master.status.wifi),
    mqtt:         Boolean(master.status.mqtt),
    espnow:       Boolean(master.status.espnow),
    fw:           master.status.fw || null,
    last_seen_at: master.status.last_seen_at || null,
    updated_at:   master.status.updated_at
  };

  return buildUpsert(
    "master_status",
    values,
    "master_id",
    ["online", "wifi", "mqtt", "espnow", "fw", "last_seen_at", "updated_at"]
  );
}

/*
 * Zweck: Stellt den SQLite-Batch für ein Gerät zusammen.
 * Schreibt immer: devices-Upsert + device_state_latest-Upsert (Snapshot).
 * Rückgabe: Transaktions-String (BEGIN IMMEDIATE … COMMIT) oder null.
 */
function buildDeviceBatch(device) {
  return wrapBatch([
    buildDeviceUpsert(device),
    buildDeviceStateLatestUpsert(device)
  ]);
}

/*
 * Zweck: Stellt den SQLite-Batch für ein Master-Gerät zusammen.
 * Schreibt bei topic_type "status": master_status-Upsert (Snapshot).
 * Rückgabe: Transaktions-String oder null.
 */
function buildMasterBatch(topicType, master) {
  if (topicType === "status" && master.status && master.status.master_id) {
    return wrapBatch([buildMasterStatusUpsert(master)]);
  }

  return null;
}

/*
 * Zweck: Zentraler Einstiegspunkt für die SQLite-Batch-Erzeugung.
 *
 * Eingaben:
 * - routed: Routing-Objekt aus topic_router (scope, topic_type)
 * - payload: verarbeitetes Gerät oder Master aus topic_handlers
 *
 * Rückgabe: Transaktions-String für den sqlite-Node oder null.
 */
function buildSqliteBatch(routed, payload) {
  if (!routed || typeof routed !== "object" || !payload || typeof payload !== "object") {
    return null;
  }

  if (routed.scope === "device" && payload.device) {
    return buildDeviceBatch(payload.device);
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
