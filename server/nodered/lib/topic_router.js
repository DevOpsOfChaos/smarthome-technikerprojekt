/**
 * =============================================================================
 * @modul     topic_router
 * @beschreibung  Zerlegt eingehende MQTT-Topics in fachliche Bestandteile und
 *                entscheidet, welcher Handler (Device/Master) zuständig ist.
 *
 * @topic-format  smarthome/<scope>/<entity_id>/<topic_type>
 *   - scope:      "device" oder "master"
 *   - entity_id:  Geräte- oder Master-Kennung
 *   - topic_type: "meta", "availability", "state", "event", "ack", "status"
 *
 * @funktionen
 *   - parseTopic         → Topic-String in Routing-Objekt zerlegen
 *   - buildRoutingState  → Vollständigen Routing-State mit Zeitstempel erzeugen
 *   - isSupportedTopic   → Prüft, ob Topic vom Server verarbeitet werden kann
 *
 * @export    parseTopic, buildRoutingState, isSupportedTopic, DEVICE_TOPICS, MASTER_TOPICS
 * =============================================================================
 */

"use strict";

const { nowIso } = require("./time_helpers");

// ===========================================================================
// DEVICE-TOPICS
// ===========================================================================
// Bekannte Gerätethemen und ihre Zielblöcke im Geräteobjekt.
// retained_expected: true → Broker soll dieses Topic retained halten
// (Metadaten und Zustand ja, Ereignisse und Acks nein).

const DEVICE_TOPICS = new Map([
  ["meta",         { target_block: "meta",          retained_expected: true  }],
  ["availability", { target_block: "availability",   retained_expected: true  }],
  ["state",        { target_block: "state",          retained_expected: true  }],
  ["event",        { target_block: "last_event",     retained_expected: false }],
  ["ack",          { target_block: "last_ack",       retained_expected: false }]
]);

// ===========================================================================
// MASTER-TOPICS
// ===========================================================================
// Gateway-Geräte haben nur einen Status-Snapshot (kein Event-Verlauf).

const MASTER_TOPICS = new Map([
  ["status", { target_block: "master_status", retained_expected: true }]
]);

// ===========================================================================
// TOPIC-PARSING
// ===========================================================================

/**
 * Zerlegt ein MQTT-Topic in seine fachlichen Bestandteile.
 *
 * Erwartetes Format: smarthome/<scope>/<entity_id>/<topic_type>
 * Beispiel: smarthome/device/net_erl_01/state
 *
 * @param {string} topic - MQTT-Topic-String
 * @returns {object|null} Routing-Objekt { scope, entity_id, topic_type, target_block, retained_expected }
 *                        oder null bei unbekanntem/ungültigem Topic
 */
function parseTopic(topic) {
  const parts = String(topic || "").split("/");
  if (parts.length !== 4 || parts[0] !== "smarthome") {
    return null;
  }

  // Gerätethemen: smarthome/device/<id>/<type>
  if (parts[1] === "device" && DEVICE_TOPICS.has(parts[3])) {
    return {
      scope: "device",
      entity_id: parts[2],
      topic_type: parts[3],
      ...DEVICE_TOPICS.get(parts[3])
    };
  }

  // Masterthemen: smarthome/master/<id>/<type>
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

/**
 * Prüft, ob ein Topic vom Server verarbeitet werden kann.
 *
 * @param {string} topic - MQTT-Topic-String
 * @returns {boolean}
 */
function isSupportedTopic(topic) {
  return Boolean(parseTopic(topic));
}

/**
 * Erzeugt den vollständigen Routing-State für eine eingehende Nachricht.
 * Fügt dem geparsten Routing-Objekt den Empfangszeitpunkt hinzu.
 *
 * @param {string} topic      - MQTT-Topic-String
 * @param {string} receivedAt - ISO-Zeitstempel des Empfangs (Standard: jetzt)
 * @returns {object|null} Routing-Objekt mit received_at, oder null
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
