"use strict";

const {
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  createRuntimeState,
  ensureDevice,
  ensureMaster
} = require("./device_store");
const { coerceTimestamp, nowIso } = require("./time_helpers");

function normalizeRuntime(runtime) {
  return runtime && typeof runtime === "object" ? runtime : createRuntimeState(nowIso());
}

function normalizeEnvelope(envelope = {}) {
  const payload = envelope.payload && typeof envelope.payload === "object" ? envelope.payload : {};
  return {
    entity_id: envelope.entity_id || payload.device_id || payload.master_id || "",
    payload,
    received_at: coerceTimestamp(envelope.received_at, nowIso())
  };
}

function handleMeta(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyMeta(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

function handleAvailability(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAvailability(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

function handleState(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyState(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

function handleEvent(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyEvent(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

function handleAck(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const device = ensureDevice(nextRuntime, message.entity_id, message.received_at);
  applyAck(device, message.payload, message.received_at);
  return { runtime: nextRuntime, device };
}

function handleMasterStatus(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const master = ensureMaster(nextRuntime, message.entity_id, message.received_at);
  applyMasterStatus(master, message.payload, message.received_at);
  return { runtime: nextRuntime, master };
}

function handleMasterEvent(runtime, envelope) {
  const nextRuntime = normalizeRuntime(runtime);
  const message = normalizeEnvelope(envelope);
  const master = ensureMaster(nextRuntime, message.entity_id, message.received_at);
  applyMasterEvent(master, message.payload, message.received_at);
  return { runtime: nextRuntime, master };
}

module.exports = {
  handleAck,
  handleAvailability,
  handleEvent,
  handleMasterEvent,
  handleMasterStatus,
  handleMeta,
  handleState
};
