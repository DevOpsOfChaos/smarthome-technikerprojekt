"use strict";

const DEVICE_TOPICS = new Map([
  ["meta", { target_block: "meta", retained_expected: true }],
  ["availability", { target_block: "availability", retained_expected: true }],
  ["state", { target_block: "state", retained_expected: true }],
  ["event", { target_block: "last_event", retained_expected: false }],
  ["ack", { target_block: "last_ack", retained_expected: false }]
]);

const MASTER_TOPICS = new Map([
  ["status", { target_block: "master_status", retained_expected: true }],
  ["event", { target_block: "master_event", retained_expected: false }]
]);

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

function isSupportedTopic(topic) {
  return Boolean(parseTopic(topic));
}

module.exports = {
  DEVICE_TOPICS,
  MASTER_TOPICS,
  isSupportedTopic,
  parseTopic
};
