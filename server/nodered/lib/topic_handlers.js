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

/*
 * Zweck: Stellt sicher, dass der globale Laufzeitzustand initialisiert ist.
 * Rückgabe: Geprüftes/initialisiertes Runtime-Objekt.
 */
function normalizeRuntime(runtime) {
  return ensureRuntime(runtime, nowIso());
}

/*
 * Zweck: Normalisiert den eingehenden Payload auf ein einfaches Objekt.
 * Verarbeitet rohe JSON-Strings, fertige Objekte und leere Payloads.
 * Rückgabe: Einfaches Objekt, niemals null oder Array.
 */
function normalizePayload(payload) {
  if (payload && typeof payload === "object" && !Array.isArray(payload)) {
    return payload;
  }

  if (typeof payload === "string") {
    try {
      const parsed = JSON.parse(payload);
      return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : {};
    } catch (error) {
      return {};
    }
  }

  return {};
}

/*
 * Zweck: Normalisiert den Nachrichtenumschlag vor der Handlerverarbeitung.
 *
 * Eingaben:
 * - envelope.entity_id: Geräte- oder Master-ID (kann auch im Payload stehen)
 * - envelope.payload: Rohpayload der MQTT-Nachricht
 * - envelope.received_at: Empfangszeitpunkt
 *
 * Rückgabe: Bereinigte Nachricht mit gesicherter entity_id und received_at.
 */
function normalizeEnvelope(envelope = {}) {
  const payload = normalizePayload(envelope.payload);
  return {
    entity_id: envelope.entity_id || payload.device_id || payload.master_id || "",
    payload,
    received_at: coerceTimestamp(envelope.received_at, nowIso())
  };
}

/*
 * Zweck: Verarbeitet eine eingehende Meta-Nachricht eines Geräts.
 * Schreibt Gerätemetadaten (Klasse, Fähigkeiten, Firmware) in das Geräteobjekt.
 * Rückgabe: { runtime, device } nach der Aktualisierung.
 */
function handleMeta(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyMeta(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/*
 * Zweck: Verarbeitet eine Verfügbarkeitsmeldung (online/offline).
 * Aktualisiert availability, online-Flag und last_seen_at im Geräteobjekt.
 * Rückgabe: { runtime, device } nach der Aktualisierung.
 */
function handleAvailability(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAvailability(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/*
 * Zweck: Verarbeitet eine Zustandsmeldung eines Geräts (Sensordaten, Aktorstatus).
 * Normalisiert alle bekannten Zustandsfelder und schreibt sie in device.state.
 * Rückgabe: { runtime, device } nach der Aktualisierung.
 */
function handleState(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyState(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/*
 * Zweck: Verarbeitet ein Geräteereignis (z. B. Tastendruck, Regenalarm).
 * Schreibt das Ereignis in device.last_event und leitet daraus ggf. Zustandsfelder ab.
 * Rückgabe: { runtime, device } nach der Aktualisierung.
 */
function handleEvent(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyEvent(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/*
 * Zweck: Verarbeitet eine Bestätigung (Ack) auf einen Steuerbefehl.
 * Schreibt Kanal, Status und Sequenznummer in device.last_ack.
 * Rückgabe: { runtime, device } nach der Aktualisierung.
 */
function handleAck(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAck(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

/*
 * Zweck: Verarbeitet eine Statusmeldung eines Master-Geräts (Gateway).
 * Aktualisiert online, WiFi, MQTT, ESP-NOW und Firmware im Master-Objekt.
 * Rückgabe: { runtime, master } nach der Aktualisierung.
 */
function handleMasterStatus(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const master = ensureMaster(nextRuntime, message.entity_id, message.received_at);
  applyMasterStatus(master, message.payload, message.received_at);
  return { runtime: nextRuntime, master };
}

/*
 * Zweck: Zentraler Eintrittspunkt für alle eingehenden MQTT-Nachrichten.
 *
 * Eingaben:
 * - runtime: aktueller Laufzeitzustand (global)
 * - routed: Routing-Objekt aus topic_router (scope, topic_type, entity_id, received_at)
 * - payload: normalisierter Nachrichteninhalt
 *
 * Rückgabe: Ergebnisobjekt mit { runtime, payload, sqlite_batch }
 *           oder null bei unbekanntem Scope oder Topic-Typ.
 *
 * Seiteneffekt: Aktualisiert das Gerät oder den Master im Runtime-State.
 *               Bereitet den SQLite-Batch für die Persistenzschicht vor.
 */
function handleRoutedMessage(runtime, routed, payload) {
  const descriptor = routed && typeof routed === "object" ? routed : {};
  const message = {
    entity_id: descriptor.entity_id,
    payload,
    received_at: descriptor.received_at
  };

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
