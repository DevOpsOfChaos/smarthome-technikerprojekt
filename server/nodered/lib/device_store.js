"use strict";

const capabilityHelpers = require("./capability_helpers");
const timeHelpers = require("./time_helpers");

// Bekannte Zustandsfelder und ihre Normalisierungsfunktion.
// Felder, die hier nicht aufgeführt sind, werden als dropped_state_fields vermerkt.
const DEVICE_STATE_NORMALIZERS = {
  fault:                normalizeBoolean,
  relay_1:              normalizeBoolean,
  relay_2:              normalizeBoolean,
  cover_mode:           normalizeTextOrNull,
  cover_state:          normalizeTextOrNull,
  cover_direction:      normalizeTextOrNull,
  cover_position:       normalizeNumber,
  cover_calibrated:     normalizeBoolean,
  is_calibrated:        normalizeBoolean,
  cover_moving:         normalizeBoolean,
  cover_target:         normalizeNumber,
  travel_time_ms:       normalizeNumber,
  temp_01c:             normalizeNumber,
  hum_01pct:            normalizeNumber,
  lux:                  normalizeNumber,
  lux_01lx:             normalizeNumber,
  pressure_pa:          normalizeNumber,
  pressure_hpa:         normalizeNumber,
  gas_ohm:              normalizeNumber,
  aqi:                  normalizeNumber,
  tvoc_ppb:             normalizeNumber,
  eco2_ppm:             normalizeNumber,
  motion:               normalizeBoolean,
  presence:             normalizeBoolean,
  rain:                 normalizeBoolean,
  rain_raw:             normalizeNumber,
  window_open:          normalizeBoolean,
  contact_open:         normalizeBoolean,
  battery_pct:          normalizeNumber,
  battery_mv:           normalizeNumber,
  button_flags:         normalizeNumber,
  button_last_action:   normalizeTextOrNull,
  button_last_action_at: normalizeTimestamp
};

// Konfigurationsfelder werden im Geräteobjekt separat von Zustandsfeldern gehalten.
const DEVICE_CONFIG_NORMALIZERS = {
  report_interval_s:      normalizeNumber,
  lux_threshold_on:       normalizeNumber,
  auto_off_delay_s:       normalizeNumber,
  rain_threshold:         normalizeNumber,
  auto_up_time:           normalizeTextOrNull,
  auto_down_time:         normalizeTextOrNull,
  auto_schedule_enabled:  normalizeBoolean
};

// Metafelder, die aus dem Payload in device.meta übernommen werden.
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

/*
 * Zweck: Erzeugt einen leeren, initialisierten Laufzeitzustand für den Server.
 * Rückgabe: Objekt mit leeren devices- und masters-Maps sowie Zeitstempel.
 */
function createRuntimeState(now = timeHelpers.nowIso()) {
  return {
    devices: {},
    masters: {},
    initialized_at: now,
    schema_version: 1
  };
}

/*
 * Zweck: Stellt sicher, dass der Laufzeitzustand vollständig und gültig ist.
 * Fehlende Teilstrukturen werden ergänzt, ohne vorhandene Daten zu überschreiben.
 * Rückgabe: Geprüftes/repariertes Runtime-Objekt.
 */
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

/*
 * Zweck: Erzeugt ein leeres Geräteobjekt für eine noch unbekannte Device-ID.
 * Der Basistyp wird aus der ID abgeleitet, falls noch keine Metadaten vorliegen.
 * Rückgabe: Vollständiges, leeres Geräteobjekt mit allen Pflichtfeldern.
 */
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

/*
 * Zweck: Erzeugt ein leeres Master-Objekt für ein noch unbekanntes Gateway.
 * Rückgabe: Vollständiges, leeres Master-Objekt mit allen Pflichtfeldern.
 */
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
    diagnostics: {
      last_handler: null,
      last_topic: null
    },
    created_at: now,
    updated_at: now,
    first_seen_at: now,
    last_seen_at: now,
    last_status_at: null
  };
}

/*
 * Zweck: Gibt das Geräteobjekt für eine Device-ID zurück.
 *
 * autoCreate steuert, ob unbekannte Geräte automatisch angelegt werden:
 * - true (Standard): Gerät wird bei erster Nachricht angelegt (MQTT-Ingest)
 * - false: null wird zurückgegeben (für lesende oder prüfende Pfade)
 *
 * Rückgabe: Geräteobjekt oder null.
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

/*
 * Zweck: Gibt das Master-Objekt für eine Master-ID zurück, legt es ggf. neu an.
 * Rückgabe: Master-Objekt oder null bei ungültiger ID.
 */
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

// Aktualisiert updated_at und last_seen_at bei jeder eingehenden Nachricht.
function touchDevice(device, receivedAt) {
  device.updated_at = receivedAt;
  device.last_seen_at = receivedAt;
}

function touchDeviceHandler(device, handlerName) {
  device.diagnostics.last_handler = handlerName;
  device.diagnostics.last_topic = handlerName;
}

/*
 * Zweck: Übernimmt Metadaten (Klasse, Fähigkeiten, Firmware) in das Geräteobjekt.
 *
 * Eingaben:
 * - device: Geräteobjekt aus dem Runtime-State
 * - payload: Rohpayload der Meta-Nachricht
 * - receivedAt: Empfangszeitpunkt
 *
 * Seiteneffekt: Aktualisiert device.meta, device.identity und leitet Capabilities neu ab.
 */
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
  // Fähigkeiten werden bei jeder Meta-Aktualisierung neu abgeleitet,
  // damit Klasse und caps-Bitmaske immer konsistent sind.
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

/*
 * Zweck: Übernimmt Verfügbarkeitsdaten (online/offline) in das Geräteobjekt.
 *
 * Eingaben:
 * - payload.availability: Statuslabel ("online", "offline", "late", …)
 * - payload.online: optionaler expliziter Boolean
 *
 * Seiteneffekt: Aktualisiert device.availability inkl. online-Flag und last_seen_at.
 */
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

  // online-Flag aus Payload übernehmen oder aus availability-Label ableiten
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

/*
 * Zweck: Übernimmt Zustandsdaten (Sensoren, Aktorstatus) in das Geräteobjekt.
 *
 * Eingabe: payload mit beliebigen Zustandsfeldern.
 *
 * Seiteneffekt:
 * - Bekannte Felder werden normalisiert und in device.state geschrieben.
 * - Konfigurationsfelder (report_interval_s, …) gehen in device.config.
 * - Unbekannte Felder werden als dropped_state_fields diagnostisch vermerkt.
 * - cover_calibrated und is_calibrated werden synchronisiert (dual-field-Kompatibilität).
 */
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

  // Beide Kalibrierungsfelder synchron halten, da Firmware beides senden kann.
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

/*
 * Zweck: Übernimmt ein Geräteereignis (z. B. Tastendruck, Alarm) in das Geräteobjekt.
 *
 * Seiteneffekt:
 * - Schreibt das normalisierte Ereignis in device.last_event.
 * - Leitet ggf. Zustandsfelder aus dem Ereignis ab (z. B. rain aus rain_detected).
 */
function applyEvent(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const eventAt = normalizeTimestamp(raw.event_at || raw.occurred_at, receivedAt) || receivedAt;
  const eventTrigger = Object.prototype.hasOwnProperty.call(raw, "event_trigger") ? raw.event_trigger : raw.trigger;

  device.last_event = {
    ...raw,
    event_type:    normalizeTextOrNull(raw.event_type),
    event_label:   normalizeTextOrNull(raw.event_label || raw.event),
    event_trigger: normalizeTextOrNull(eventTrigger),
    param1:        normalizeTextOrNull(raw.param1),
    param2:        normalizeTextOrNull(raw.param2),
    event_at:      eventAt
  };

  device.updated_at = receivedAt;
  device.last_event_at = eventAt;
  applyStateFromEvent(device);
  touchDeviceHandler(device, "event");
  return device;
}

function booleanFromEventParam(value) {
  if (value === undefined || value === null || value === "") {
    return null;
  }

  if (typeof value === "boolean") {
    return value;
  }

  if (typeof value === "number") {
    return value !== 0;
  }

  const text = String(value).trim().toLowerCase();
  if (["1", "true", "yes", "on", "wet", "nass"].includes(text)) {
    return true;
  }
  if (["0", "false", "no", "off", "dry", "trocken"].includes(text)) {
    return false;
  }

  return null;
}

function rememberDerivedStateField(device, fieldName, eventLabel) {
  if (!isPlainObject(device.diagnostics)) {
    device.diagnostics = {};
  }

  const existing = Array.isArray(device.diagnostics.derived_state_fields)
    ? device.diagnostics.derived_state_fields
    : [];
  const entry = `${fieldName}:event:${eventLabel}`;
  if (!existing.includes(entry)) {
    device.diagnostics.derived_state_fields = [...existing, entry];
  }
}

/*
 * Zweck: Leitet Zustandsfelder aus bestimmten Ereignissen ab.
 *
 * Derzeit implementiert: rain_detected → device.state.rain
 * Erweiterbar für weitere ereignisbasierte Zustandsableitungen.
 */
function applyStateFromEvent(device) {
  const event = cloneObject(device && device.last_event);
  const eventLabel = normalizeTextOrNull(event.event_label || event.event);
  if (eventLabel !== "rain_detected") {
    return;
  }

  const rainState = booleanFromEventParam(event.param1 ?? event.param2);
  if (rainState === null) {
    return;
  }

  device.state.rain = rainState;
  rememberDerivedStateField(device, "rain", eventLabel);
}

/*
 * Zweck: Übernimmt eine Befehlsbestätigung (Ack) in das Geräteobjekt.
 *
 * Seiteneffekt: Schreibt request_id, channel, status, status_code und ack_seq
 *               in device.last_ack für die UI-Rückmeldung.
 */
function applyAck(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const ackAt = normalizeTimestamp(raw.ack_at || raw.occurred_at, receivedAt) || receivedAt;

  device.last_ack = {
    ...raw,
    request_id:   normalizeTextOrNull(raw.request_id),
    channel:      normalizeTextOrNull(raw.channel),
    status:       normalizeTextOrNull(raw.status),
    status_code:  normalizeTextOrNull(raw.status_code),
    ack_msg_type: normalizeTextOrNull(raw.ack_msg_type),
    ack_seq:      normalizeTextOrNull(raw.ack_seq),
    source:       normalizeTextOrNull(raw.source),
    ack_at:       ackAt
  };

  device.updated_at = receivedAt;
  device.last_ack_at = ackAt;
  touchDeviceHandler(device, "ack");
  return device;
}

/*
 * Zweck: Übernimmt Statusdaten eines Master-Geräts (Gateway).
 * Seiteneffekt: Aktualisiert online, wifi, mqtt, espnow, fw und last_seen_at.
 */
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
    online:    timeHelpers.coerceBoolean(raw.online,  master.status.online  || false),
    wifi:      timeHelpers.coerceBoolean(raw.wifi,    master.status.wifi    || false),
    mqtt:      timeHelpers.coerceBoolean(raw.mqtt,    master.status.mqtt    || false),
    espnow:    timeHelpers.coerceBoolean(raw.espnow,  master.status.espnow  || false),
    fw:        normalizeTextOrNull(raw.fw) || master.status.fw || null,
    last_seen_at: lastSeenAt,
    updated_at:   receivedAt
  };

  master.updated_at = receivedAt;
  master.last_seen_at = lastSeenAt;
  master.last_status_at = receivedAt;
  master.diagnostics.last_handler = "status";
  master.diagnostics.last_topic = "status";
  return master;
}


function textOrNull(value) {
  return value === undefined || value === null ? null : String(value);
}

module.exports = {
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  createRuntimeState,
  ensureDevice,
  ensureMaster,
  ensureRuntime
};
