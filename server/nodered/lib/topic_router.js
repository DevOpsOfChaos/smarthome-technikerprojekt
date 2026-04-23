"use strict";

const { nowIso } = require("./time_helpers");

// Bekannte Gerätethemen und ihre Zielblöcke im Geräteobjekt.
// retained_expected markiert, ob der Broker dieses Topic retained halten soll
// (Metadaten und Zustand ja, Ereignisse und Acks nein).
const DEVICE_TOPICS = new Map([
  ["meta",         { target_block: "meta",          retained_expected: true  }],
  ["availability", { target_block: "availability",   retained_expected: true  }],
  ["state",        { target_block: "state",          retained_expected: true  }],
  ["event",        { target_block: "last_event",     retained_expected: false }],
  ["ack",          { target_block: "last_ack",       retained_expected: false }]
]);

// Bekannte Master-Themen (Gateway-Geräte). Nur Status-Snapshot, kein Event-Verlauf.
const MASTER_TOPICS = new Map([
  ["status", { target_block: "master_status", retained_expected: true }]
]);

/*
 * Zweck: Zerlegt ein MQTT-Topic in seine fachlichen Bestandteile.
 *
 * Erwartetes Format: smarthome/<scope>/<entity_id>/<topic_type>
 * - scope: "device" oder "master"
 * - entity_id: Geräte- oder Master-ID
 * - topic_type: z. B. "state", "meta", "availability"
 *
 * Rückgabe: Routing-Objekt mit scope, entity_id, topic_type und Zielblock,
 *           oder null bei unbekanntem oder ungültigem Topic.
 */
function parseTopic(topic) {
  const parts = String(topic || "").split("/");
  if (parts.length !== 4 || parts[0] !== "smarthome") {
    return null;
  }

  if (parts[1] === "device" && DEVICE_TOPICS.has(parts[3])) {
    return {
      scope: "device",
      entity_id: parts[2],
      topic_type: parts[3],
      ...DEVICE_TOPICS.get(parts[3])
    };
  }

  if (parts[1] === "master" && MASTER_TOPICS.has(parts[3])) {
    return {
      scope: "master",
      entity_id: parts[2],
      topic_type: parts[3],
      ...MASTER_TOPICS.get(parts[3])
    };
  }

  return null;
}

/*
 * Zweck: Prüft, ob ein Topic vom Server verarbeitet werden kann.
 * Rückgabe: true bei bekanntem Topic, false sonst.
 */
function isSupportedTopic(topic) {
  return Boolean(parseTopic(topic));
}

/*
 * Zweck: Erzeugt den vollständigen Routing-State für eine eingehende Nachricht.
 *
 * Eingaben:
 * - topic: MQTT-Topic-String
 * - receivedAt: ISO-Zeitstempel des Empfangs (Standard: jetzt)
 *
 * Rückgabe: Routing-Objekt mit allen Feldern aus parseTopic
 *           plus received_at, oder null bei unbekanntem Topic.
 */
function buildRoutingState(topic, receivedAt = nowIso()) {
  const parsed = parseTopic(topic);
  return parsed
    ? {
        ...parsed,
        received_at: receivedAt
      }
    : null;
}

module.exports = {
  buildRoutingState,
  DEVICE_TOPICS,
  MASTER_TOPICS,
  isSupportedTopic,
  parseTopic
};
