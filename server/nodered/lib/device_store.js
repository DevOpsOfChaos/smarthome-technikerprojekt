/**
 * =============================================================================
 * @modul     device_store
 * @beschreibung  Zentraler Laufzeitzustand (Runtime-State) des Servers.
 *                Hält alle Geräte und Master-Gateways im Speicher, normalisiert
 *                eingehende MQTT-Daten und stellt Zugriffsfunktionen bereit.
 *
 * @funktionen
 *   - createRuntimeState / ensureRuntime   → Laufzeitzustand initialisieren
 *   - createEmptyDevice / createEmptyMaster → Leere Datenstrukturen anlegen
 *   - ensureDevice / ensureMaster           → Geräte/Master abrufen oder anlegen
 *   - applyMeta / applyAvailability         → Metadaten und Verfügbarkeit schreiben
 *   - applyState / applyEvent / applyAck    → Zustand, Ereignisse, Bestätigungen
 *   - applyMasterStatus                     → Gateway-Status schreiben
 *
 * @nutzung   Wird von topic_handlers.js und den Flow-Funktionen verwendet.
 * @export    createRuntimeState, ensureRuntime, ensureDevice, ensureMaster,
 *            applyMeta, applyAvailability, applyState, applyEvent, applyAck,
 *            applyMasterStatus
 * =============================================================================
 */

"use strict";

const capabilityHelpers = require("./capability_helpers");
const timeHelpers = require("./time_helpers");

// ===========================================================================
// NORMALISIERER FÜR GERÄTEZUSTÄNDE
// ===========================================================================
// Jeder Eintrag ordnet einem Zustandsfeldnamen seine Normalisierungsfunktion zu.
// Felder, die hier nicht aufgeführt sind, werden als dropped_state_fields vermerkt
// – wichtig für die Diagnose unerwarteter Firmware-Felder.

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

// ===========================================================================
// NORMALISIERER FÜR GERÄTEKONFIGURATION
// ===========================================================================
// Konfigurationsfelder werden separat von Zustandsfeldern im device.config
// gehalten – sie beschreiben Geräteverhalten, nicht Sensorwerte.

const DEVICE_CONFIG_NORMALIZERS = {
  report_interval_s:      normalizeNumber,
  lux_threshold_on:       normalizeNumber,
  auto_off_delay_s:       normalizeNumber,
  rain_threshold:         normalizeNumber,
  auto_up_time:           normalizeTextOrNull,
  auto_down_time:         normalizeTextOrNull,
  auto_schedule_enabled:  normalizeBoolean
};

// ===========================================================================
// METAFELDER
// ===========================================================================
// Diese Felder werden aus dem Payload eins-zu-eins in device.meta übernommen.
// Sie stammen aus der Geräte-Selbstauskunft (Firmware-Broadcast nach Verbindung).

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

// ===========================================================================
// PRIVATE NORMALISIERUNGSFUNKTIONEN
// ===========================================================================

/**
 * Normalisiert auf String oder null.
 * Leere Strings und reine Whitespace-Strings werden zu null.
 */
function normalizeTextOrNull(value) {
  if (value === undefined || value === null) {
    return null;
  }

  const text = String(value).trim();
  return text ? text : null;
}

/**
 * Normalisiert auf Boolean oder null.
 * Nutzt die zentrale coerceBoolean-Funktion, gibt aber bei leeren Werten null zurück.
 */
function normalizeBoolean(value) {
  if (value === undefined || value === null || value === "") {
    return null;
  }

  return timeHelpers.coerceBoolean(value, false);
}

/**
 * Normalisiert auf endliche Zahl oder null.
 */
function normalizeNumber(value) {
  return timeHelpers.coerceNumber(value, null);
}

/**
 * Normalisiert einen Zeitstempel auf ISO-8601 oder Fallback.
 * Leere Werte → Fallback (Standard: jetzt).
 */
function normalizeTimestamp(value, fallback) {
  if (value === undefined || value === null || value === "") {
    return fallback || null;
  }

  return timeHelpers.coerceTimestamp(value, fallback || timeHelpers.nowIso());
}

/**
 * Klont ein einfaches Objekt flach. Gibt leeres Objekt zurück, wenn Eingabe kein Objekt ist.
 */
function cloneObject(input) {
  return timeHelpers.isPlainObject(input) ? { ...input } : {};
}

// ===========================================================================
// LAUFZEITZUSTAND – INITIALISIERUNG
// ===========================================================================

/**
 * Erzeugt einen leeren, initialisierten Laufzeitzustand.
 * Wird beim Serverstart und bei vollständig ungültigem Runtime-State aufgerufen.
 *
 * @param {string} now - ISO-Zeitstempel (Standard: jetzt)
 * @returns {object} Runtime-State mit leeren devices-/masters-Maps
 */
function createRuntimeState(now = timeHelpers.nowIso()) {
  return {
    devices: {},
    masters: {},
    initialized_at: now,
    schema_version: 1
  };
}

/**
 * Stellt sicher, dass der Laufzeitzustand vollständig und gültig ist.
 * Fehlende Teilstrukturen werden ergänzt – vorhandene Daten bleiben unangetastet.
 * Wird vor jedem Zugriff auf den Runtime-State aufgerufen.
 *
 * @param {object} runtimeState - Aktueller (ggf. unvollständiger) State
 * @param {string} now          - ISO-Zeitstempel
 * @returns {object} Geprüfter/reparierter Runtime-State
 */
function ensureRuntime(runtimeState, now = timeHelpers.nowIso()) {
  if (!timeHelpers.isPlainObject(runtimeState)) {
    return createRuntimeState(now);
  }

  if (!timeHelpers.isPlainObject(runtimeState.devices)) {
    runtimeState.devices = {};
  }

  if (!timeHelpers.isPlainObject(runtimeState.masters)) {
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

// ===========================================================================
// LAUFZEITZUSTAND – GERÄTE UND MASTER
// ===========================================================================

/**
 * Erzeugt ein leeres Geräteobjekt für eine noch unbekannte Device-ID.
 * Der Basistyp wird aus der ID abgeleitet, falls noch keine Metadaten vorliegen.
 * Das Objekt enthält alle Pflichtfelder für spätere meta/state/event/ack-Updates.
 *
 * @param {string} deviceId - Eindeutige Gerätekennung
 * @param {string} now      - ISO-Zeitstempel
 * @returns {object} Vollständiges, leeres Geräteobjekt
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

/**
 * Erzeugt ein leeres Master-Objekt (Gateway) für eine unbekannte Master-ID.
 *
 * @param {string} masterId - Eindeutige Master-Kennung
 * @param {string} now      - ISO-Zeitstempel
 * @returns {object} Vollständiges, leeres Master-Objekt
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

/**
 * Gibt das Geräteobjekt für eine Device-ID zurück und legt es bei Bedarf neu an.
 *
 * Steuerparameter autoCreate (in options):
 * - true  (Standard): Gerät wird bei erster Nachricht automatisch angelegt (MQTT-Ingest)
 * - false: null wird zurückgegeben (für lesende oder prüfende Pfade)
 *
 * @param {object} runtimeState - Laufzeitzustand
 * @param {string} deviceId     - Gerätekennung
 * @param {string} now          - ISO-Zeitstempel
 * @param {object} options      - { autoCreate: boolean }
 * @returns {object|null} Geräteobjekt oder null
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

/**
 * Gibt das Master-Objekt für eine Master-ID zurück, legt es ggf. neu an.
 *
 * @param {object} runtimeState - Laufzeitzustand
 * @param {string} masterId     - Master-Kennung
 * @param {string} now          - ISO-Zeitstempel
 * @returns {object|null} Master-Objekt oder null
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

// ===========================================================================
// LAUFZEITZUSTAND – INTERNE HELFER
// ===========================================================================

/**
 * Aktualisiert die Zeitstempel bei jeder eingehenden Nachricht.
 * updated_at und last_seen_at werden auf den Empfangszeitpunkt gesetzt.
 */
function touchDevice(device, receivedAt) {
  device.updated_at = receivedAt;
  device.last_seen_at = receivedAt;
}

/**
 * Vermerkt den letzten Handler im Diagnosebereich des Geräts.
 */
function touchDeviceHandler(device, handlerName) {
  device.diagnostics.last_handler = handlerName;
  device.diagnostics.last_topic = handlerName;
}

// ===========================================================================
// DATENÜBERNAHME – GERÄT
// ===========================================================================

/**
 * Übernimmt Metadaten (Klasse, Fähigkeiten, Firmware-Version) in das Geräteobjekt.
 *
 * Eingang:  Payload der Meta-Nachricht vom Gerät.
 * Wirkung:  device.meta wird aktualisiert, device.identity neu abgeleitet.
 *           Fähigkeiten (caps) werden bei jeder Meta-Aktualisierung neu aus Klasse
 *           und Bitmaske berechnet – damit Klasse und Caps immer konsistent sind.
 *
 * @param {object} device     - Geräteobjekt aus dem Runtime-State
 * @param {object} payload    - Rohpayload der Meta-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Gerät
 */
function applyMeta(device, payload, receivedAt = timeHelpers.nowIso()) {
  if (!device) {
    return null;
  }

  const raw = cloneObject(payload);
  const nextMeta = { ...cloneObject(device.meta), ...raw };

  // Bekannte Metafelder aus Payload übernehmen und normalisieren.
  DEVICE_META_FIELDS.forEach((fieldName) => {
    if (Object.prototype.hasOwnProperty.call(raw, fieldName)) {
      nextMeta[fieldName] = normalizeTextOrNull(raw[fieldName]);
    }
  });

  // sim_case ist ein Sonderfall – kann boolesch oder String sein.
  if (Object.prototype.hasOwnProperty.call(raw, "sim_case")) {
    nextMeta.sim_case = raw.sim_case;
  }

  nextMeta.device_id = device.identity.device_id;
  nextMeta.device_name = normalizeTextOrNull(nextMeta.device_name) || device.identity.device_name;
  nextMeta.device_class = normalizeTextOrNull(nextMeta.device_class);

  // Fähigkeiten aus device_class + caps-Bitmaske neu ableiten.
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

/**
 * Übernimmt Verfügbarkeitsdaten (online/offline/late) in das Geräteobjekt.
 *
 * Eingang:  payload.availability (Statuslabel), payload.online (Boolean)
 * Wirkung:  device.availability wird aktualisiert. Das online-Flag wird
 *           aus dem Payload oder aus dem availability-Label abgeleitet.
 *
 * @param {object} device     - Geräteobjekt
 * @param {object} payload    - Rohpayload der Availability-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Gerät
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

  // online-Flag: explizite Angabe > Label-Ableitung > vorheriger Wert > false
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

/**
 * Übernimmt Zustandsdaten (Sensoren, Aktorstatus) in das Geräteobjekt.
 *
 * Eingang:  Payload mit beliebigen Zustandsfeldern (temp_01c, relay_1, lux, …).
 * Wirkung:
 *   - Bekannte Felder werden normalisiert und in device.state geschrieben.
 *   - Konfigurationsfelder gehen in device.config.
 *   - Unbekannte Felder landen in den Diagnosedaten (dropped_state_fields).
 *   - cover_calibrated und is_calibrated werden synchronisiert (Dual-Field-Kompatibilität).
 *
 * @param {object} device     - Geräteobjekt
 * @param {object} payload    - Rohpayload der State-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Gerät
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

    // Bekanntes Zustandsfeld → normalisieren und in state schreiben.
    if (Object.prototype.hasOwnProperty.call(DEVICE_STATE_NORMALIZERS, fieldName)) {
      device.state[fieldName] = DEVICE_STATE_NORMALIZERS[fieldName](fieldValue);
      return;
    }

    // Konfigurationsfeld → normalisieren und in config schreiben.
    if (Object.prototype.hasOwnProperty.call(DEVICE_CONFIG_NORMALIZERS, fieldName)) {
      device.config[fieldName] = DEVICE_CONFIG_NORMALIZERS[fieldName](fieldValue);
      return;
    }

    // Unbekanntes Feld → für Diagnose vermerken.
    droppedStateFields.push(fieldName);
  });

  // Dual-Field-Synchronisation: cover_calibrated und is_calibrated müssen
  // zwingend denselben Wert haben, da Firmware beide senden kann.
  const coverCalibrated = device.state.cover_calibrated ?? device.state.is_calibrated;
  if (coverCalibrated !== undefined) {
    device.state.cover_calibrated = coverCalibrated;
    device.state.is_calibrated = coverCalibrated;
  }

  // Verfügbarkeitsstruktur sicherstellen – last_seen_at wird immer aktualisiert.
  if (!timeHelpers.isPlainObject(device.availability)) {
    device.availability = { availability: "unknown", online: false, last_seen_at: null };
  }
  device.availability.last_seen_at = receivedAt;

  touchDevice(device, receivedAt);
  device.last_state_at = receivedAt;
  device.diagnostics.dropped_state_fields = droppedStateFields;
  touchDeviceHandler(device, "state");
  return device;
}

/**
 * Übernimmt ein Geräteereignis (Tastendruck, Regenalarm, Fehler) in das Geräteobjekt.
 *
 * Eingang:  Payload mit event_type, event_label, event_trigger, Parametern.
 * Wirkung:
 *   - Das normalisierte Ereignis wird in device.last_event geschrieben.
 *   - Ggf. werden Zustandsfelder aus dem Ereignis abgeleitet (z. B. rain aus rain_detected).
 *
 * @param {object} device     - Geräteobjekt
 * @param {object} payload    - Rohpayload der Event-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Gerät
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

// ===========================================================================
// EREIGNISBASIERTE ZUSTANDSABLEITUNG
// ===========================================================================

/**
 * Versucht, aus einem Event-Parameter einen Boolean zu lesen.
 * Versteht: booleans, Zahlen (0/nicht-0), und textuelle Werte
 * ("wet"/"nass" → true, "dry"/"trocken" → false, usw.).
 */
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

/**
 * Vermerkt ein abgeleitetes Zustandsfeld in den Diagnosedaten.
 * Format: "feldname:event:event_label" (z. B. "rain:event:rain_detected").
 */
function rememberDerivedStateField(device, fieldName, eventLabel) {
  if (!timeHelpers.isPlainObject(device.diagnostics)) {
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

/**
 * Leitet Zustandsfelder aus bestimmten Ereignissen ab.
 *
 * Derzeit implementiert: rain_detected → device.state.rain
 * Die Funktion ist bewusst erweiterbar für weitere Ereignistypen.
 * Kein Seiteneffekt außerhalb des übergebenen device-Objekts.
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

// ===========================================================================
// DATENÜBERNAHME – BESTÄTIGUNGEN & MASTER
// ===========================================================================

/**
 * Übernimmt eine Befehlsbestätigung (Ack) vom Gerät in das Geräteobjekt.
 *
 * Eingang:  Payload mit request_id, channel, status, status_code, ack_seq usw.
 * Wirkung:  device.last_ack wird gesetzt. Dient der UI als Rückmeldung,
 *           ob ein gesendeter Befehl das Gerät erreicht hat.
 *
 * @param {object} device     - Geräteobjekt
 * @param {object} payload    - Rohpayload der Ack-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Gerät
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

/**
 * Übernimmt Statusdaten eines Master-Geräts (Gateway).
 *
 * Eingang:  Payload mit online, wifi, mqtt, espnow, fw.
 * Wirkung:  master.status wird mit normalisierten Werten aktualisiert.
 *           Alle Verbindungsarten werden als Boolean geprüft und gesetzt.
 *
 * @param {object} master     - Master-Objekt
 * @param {object} payload    - Rohpayload der Status-Nachricht
 * @param {string} receivedAt - Empfangszeitpunkt
 * @returns {object|null} Aktualisiertes Master-Objekt
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

// Helfer für sqlite_writes.js – konvertiert Wert zu String oder null.
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
