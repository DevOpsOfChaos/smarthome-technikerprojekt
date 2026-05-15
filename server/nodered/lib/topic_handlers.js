/**
 * =============================================================================
 * @modul     topic_handlers
 * @beschreibung  Verarbeitet eingehende MQTT-Nachrichten und schreibt sie in
 *                den Laufzeitzustand (Runtime-State). Bindeglied zwischen
 *                MQTT-Ingest und der Persistenzschicht.
 *
 * @handler-pro-topic
 *   - handleMeta           → Gerätemetadaten (Klasse, Fähigkeiten, Firmware)
 *   - handleAvailability   → Verfügbarkeit (online/offline/late)
 *   - handleState          → Sensordaten und Aktorstatus
 *   - handleEvent          → Geräteereignisse (Tastendruck, Regenalarm)
 *   - handleAck            → Befehlsbestätigungen
 *   - handleMasterStatus   → Gateway-Status (WiFi, MQTT, ESP-NOW)
 *
 * @einstiegspunkt   handleRoutedMessage – zentraler Dispatcher für alle Topics
 *
 * @nutzung    20_device_store.json, 90_master_diag.json (Flow-Funktionen)
 * @export     handleMeta, handleAvailability, handleState, handleEvent, handleAck,
 *             handleMasterStatus, handleRoutedMessage, normalizeRuntime
 * =============================================================================
 */

"use strict";

const {
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  ensureRuntime,
  ensureDevice,
  ensureMaster
} = require("./device_store");
const sqliteWrites = require("./sqlite_writes");
const { coerceTimestamp, nowIso } = require("./time_helpers");

// ===========================================================================
// EINGANGS-NORMALISIERUNG
// ===========================================================================

/**
 * Stellt sicher, dass der globale Laufzeitzustand initialisiert ist.
 * Wird vor jedem Handler-Aufruf ausgeführt.
 *
 * @param {object} runtime - Aktueller (ggf. leerer) Laufzeitzustand
 * @returns {object} Geprüfter/initialisierter Runtime-State
 */
function normalizeRuntime(runtime) {
  return ensureRuntime(runtime, nowIso());
}

/**
 * Normalisiert den eingehenden Payload auf ein einfaches Objekt.
 * Verarbeitet rohe JSON-Strings, fertige Objekte und leere Payloads.
 * Niemals null oder Array – immer mindestens {}.
 *
 * @param {*} payload - Roher Nachrichteninhalt
 * @returns {object} Einfaches Objekt
 */
function normalizePayload(payload) {
  if (payload && typeof payload === "object" && !Array.isArray(payload)) {
    return payload;
  }

  if (typeof payload === "string") {
    try {
      const parsed = JSON.parse(payload);
      return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : {};
    } catch (_error) {
      return {};
    }
  }

  return {};
}

/**
 * Normalisiert den Nachrichtenumschlag vor der Handlerverarbeitung.
 *
 * @param {object} envelope - { entity_id, payload, received_at }
 * @returns {object} Bereinigte Nachricht mit entity_id und received_at
 */
function normalizeEnvelope(envelope = {}) {
  const payload = normalizePayload(envelope.payload);
  return {
    entity_id: envelope.entity_id || payload.device_id || payload.master_id || "",
    payload,
    received_at: coerceTimestamp(envelope.received_at, nowIso())
  };
}

// ===========================================================================
// DEVICE-HANDLER
// ===========================================================================

/**
 * Verarbeitet eine eingehende Meta-Nachricht eines Geräts.
 * Schreibt Gerätemetadaten (Klasse, Fähigkeiten, Firmware) in das Geräteobjekt.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, device: object }}
 */
function handleMeta(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyMeta(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/**
 * Verarbeitet eine Verfügbarkeitsmeldung (online/offline/late).
 * Aktualisiert availability, online-Flag und last_seen_at im Geräteobjekt.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, device: object }}
 */
function handleAvailability(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAvailability(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/**
 * Verarbeitet eine Zustandsmeldung (Sensordaten, Aktorstatus).
 * Normalisiert alle bekannten Zustandsfelder und schreibt sie in device.state.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, device: object }}
 */
function handleState(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyState(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/**
 * Verarbeitet ein Geräteereignis (Tastendruck, Regenalarm).
 * Schreibt das Ereignis in device.last_event und leitet ggf. Zustandsfelder ab.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, device: object }}
 */
function handleEvent(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyEvent(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/**
 * Verarbeitet eine Befehlsbestätigung (Ack) vom Gerät.
 * Schreibt Kanal, Status und Sequenznummer in device.last_ack.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, device: object }}
 */
function handleAck(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAck(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

// ===========================================================================
// MASTER-HANDLER
// ===========================================================================

/**
 * Verarbeitet eine Statusmeldung eines Master-Geräts (Gateway).
 * Aktualisiert online, WiFi, MQTT, ESP-NOW und Firmware im Master-Objekt.
 *
 * @param {object} runtime  - Aktueller Laufzeitzustand
 * @param {object} envelope - Nachrichtenumschlag
 * @returns {{ runtime: object, master: object }}
 */
function handleMasterStatus(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const master = ensureMaster(nextRuntime, message.entity_id, message.received_at);
  applyMasterStatus(master, message.payload, message.received_at);
  return { runtime: nextRuntime, master };
}

// ===========================================================================
// ZENTRALER DISPATCHER
// ===========================================================================

/**
 * Zentraler Eintrittspunkt für alle eingehenden MQTT-Nachrichten.
 *
 * Ablauf:
 *   1. Routing-Deskriptor auswerten (scope, topic_type)
 *   2. Passenden Handler aus der Handler-Map auswählen
 *   3. Handler ausführen → Geräte-/Master-Objekt aktualisieren
 *   4. SQLite-Batch für die Persistenzschicht vorbereiten
 *   5. Ergebnis mit aktualisiertem Runtime-State und Batch zurückgeben
 *
 * @param {object} runtime - Aktueller Laufzeitzustand (global.get("smarthome_runtime"))
 * @param {object} routed  - Routing-Objekt aus topic_router (scope, topic_type, entity_id, received_at)
 * @param {object} payload - Normalisierter Nachrichteninhalt
 * @returns {object|null} Ergebnis { runtime, payload, sqlite_batch } oder null
 */
function handleRoutedMessage(runtime, routed, payload) {
  const descriptor = routed && typeof routed === "object" ? routed : {};
  const message = {
    entity_id: descriptor.entity_id,
    payload,
    received_at: descriptor.received_at
  };

  // ---- Device-Scope ----
  if (descriptor.scope === "device") {
    const deviceHandlers = {
      meta:         handleMeta,
      availability: handleAvailability,
      state:        handleState,
      event:        handleEvent,
      ack:          handleAck
    };
    const handler = deviceHandlers[descriptor.topic_type];
    if (!handler) {
      return null;
    }

    const result = handler(runtime, message);
    if (!result || !result.device) {
      return null;
    }

    const payloadResult = {
      device_id: result.device.identity.device_id,
      block: descriptor.topic_type,
      device: result.device
    };

    return {
      ...result,
      payload: payloadResult,
      sqlite_batch: sqliteWrites.buildSqliteBatch(descriptor, payloadResult)
    };
  }

  // ---- Master-Scope ----
  if (descriptor.scope === "master") {
    const masterHandlers = {
      status: handleMasterStatus
    };
    const handler = masterHandlers[descriptor.topic_type];
    if (!handler) {
      return null;
    }

    const result = handler(runtime, message);
    if (!result || !result.master) {
      return null;
    }

    const payloadResult = {
      master_id: result.master.status.master_id,
      block: descriptor.topic_type,
      master: result.master
    };

    return {
      ...result,
      payload: payloadResult,
      sqlite_batch: sqliteWrites.buildSqliteBatch(descriptor, payloadResult)
    };
  }

  return null;
}

module.exports = {
  handleAck,
  handleAvailability,
  handleEvent,
  handleMasterStatus,
  handleMeta,
  handleRoutedMessage,
  handleState,
  normalizeRuntime
};
