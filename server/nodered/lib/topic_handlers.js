"use strict";

const {
  applyAck,
  applyAvailability,
  applyEvent,
  applyMasterEvent,
  applyMasterStatus,
  applyMeta,
  applyState,
  ensureRuntime,
  ensureDevice,
  ensureMaster
} = require("./device_store");
const sqliteWrites = require("./sqlite_writes");
const { coerceTimestamp, nowIso } = require("./time_helpers");

function normalizeRuntime(runtime) {
  return ensureRuntime(runtime, nowIso());
}

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

function normalizeEnvelope(envelope = {}) {
  const payload = normalizePayload(envelope.payload);
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

function handleRoutedMessage(runtime, routed, payload) {
  const descriptor = routed && typeof routed === "object" ? routed : {};
  const message = {
    entity_id: descriptor.entity_id,
    payload,
    received_at: descriptor.received_at
  };

  if (descriptor.scope === "device") {
    const deviceHandlers = {
      meta: handleMeta,
      availability: handleAvailability,
      state: handleState,
      event: handleEvent,
      ack: handleAck
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
      status: handleMasterStatus,
      event: handleMasterEvent
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
  handleMasterEvent,
  handleMasterStatus,
  handleMeta,
  handleRoutedMessage,
  handleState,
  normalizeRuntime
};
