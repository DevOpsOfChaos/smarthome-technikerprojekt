/*
 * basetypes/master_compat/main.cpp
 *
 * Kompatibler Masterpfad – Stage 1
 * MQTT/JSON-Vertrag vollständig, eine simulierte net_zrl-Node.
 * Kein echtes ESP-NOW in dieser Stage.
 *
 * Vertrag: smarthome-technikerprojekt, master_vertrag_bestandsaufnahme
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define MQTT_BUF_SIZE       1024
#define RECONNECT_DELAY_MS  3000

static const char TOPIC_MASTER_STATUS[] =
    "smarthome/master/" CONF_MASTER_ID "/status";

static char TOPIC_NODE_META[64];
static char TOPIC_NODE_AVAIL[64];
static char TOPIC_NODE_STATE[64];
static char TOPIC_NODE_ACK[64];
static char TOPIC_NODE_CMD_SUB[] = "smarthome/device/+/command";

struct NetZrlNode {
    const char* device_id;
    const char* device_name;
    bool relay_1;
    bool relay_2;
    bool cover_mode;
    char cover_state[16];
    bool cover_calibrated;
    int cover_position;
    bool fault;
    bool available;
    uint32_t ack_seq;
};

static NetZrlNode g_node;

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static char g_mac[18];
static uint32_t g_reconnect_ts = 0;

static void wifiConnect();
static void mqttConnect();
static void publishAll();
static void publishMasterStatus(bool online);
static void publishNodeMeta();
static void publishNodeAvailability();
static void publishNodeState();
static void publishNodeAck(const char* device_id, const char* request_id,
                           const char* status, int status_code,
                           uint8_t ack_msg_type, const char* source);
static void onMqttMessage(const char* topic, byte* payload, unsigned int len);
static void handleCommand(const char* device_id,
                          const char* payload_str, unsigned int len);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[master_compat] boot");

    g_node.device_id = CONF_NODE_ID;
    g_node.device_name = CONF_NODE_NAME;
    g_node.relay_1 = false;
    g_node.relay_2 = false;
    g_node.cover_mode = true;
    strlcpy(g_node.cover_state, "stopped", sizeof(g_node.cover_state));
    g_node.cover_calibrated = false;
    g_node.cover_position = -1;
    g_node.fault = false;
    g_node.available = true;
    g_node.ack_seq = 0;

    snprintf(TOPIC_NODE_META, sizeof(TOPIC_NODE_META),
             "smarthome/device/%s/meta", CONF_NODE_ID);
    snprintf(TOPIC_NODE_AVAIL, sizeof(TOPIC_NODE_AVAIL),
             "smarthome/device/%s/availability", CONF_NODE_ID);
    snprintf(TOPIC_NODE_STATE, sizeof(TOPIC_NODE_STATE),
             "smarthome/device/%s/state", CONF_NODE_ID);
    snprintf(TOPIC_NODE_ACK, sizeof(TOPIC_NODE_ACK),
             "smarthome/device/%s/ack", CONF_NODE_ID);

    wifiConnect();

    mqtt.setServer(CONF_MQTT_HOST, CONF_MQTT_PORT);
    mqtt.setBufferSize(MQTT_BUF_SIZE);
    mqtt.setCallback(onMqttMessage);

    mqttConnect();
}

void loop() {
    if (!mqtt.connected()) {
        uint32_t now = millis();
        if (now - g_reconnect_ts >= RECONNECT_DELAY_MS) {
            g_reconnect_ts = now;
            if (WiFi.status() != WL_CONNECTED) {
                wifiConnect();
            }
            mqttConnect();
        }
        return;
    }
    mqtt.loop();
}

static void wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(CONF_WIFI_SSID, CONF_WIFI_PASS);
    Serial.print("[wifi] connecting");
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.macAddress().toCharArray(g_mac, sizeof(g_mac));
        Serial.print("[wifi] IP=");
        Serial.print(WiFi.localIP());
        Serial.print("  MAC=");
        Serial.println(g_mac);
    } else {
        Serial.println("[wifi] connection failed - will retry in loop");
        strlcpy(g_mac, "00:00:00:00:00:00", sizeof(g_mac));
    }
}

static void mqttConnect() {
    StaticJsonDocument<256> lwtDoc;
    lwtDoc["master_id"] = CONF_MASTER_ID;
    lwtDoc["online"] = false;
    lwtDoc["wifi"] = false;
    lwtDoc["mqtt"] = false;
    lwtDoc["espnow"] = false;
    lwtDoc["fw"] = CONF_FW_VERSION;
    char lwtBuf[256];
    serializeJson(lwtDoc, lwtBuf, sizeof(lwtBuf));

    Serial.print("[mqtt] connecting");
    bool connected = false;

    for (uint8_t i = 0; i < 5; i++) {
        const char* user = strlen(CONF_MQTT_USER) > 0 ? CONF_MQTT_USER : nullptr;
        const char* pass = strlen(CONF_MQTT_PASS) > 0 ? CONF_MQTT_PASS : nullptr;

        connected = mqtt.connect(
            CONF_MASTER_ID,
            user,
            pass,
            TOPIC_MASTER_STATUS,
            1,
            true,
            lwtBuf
        );
        if (connected) {
            break;
        }
        Serial.print(".");
        delay(1500);
    }

    if (!connected) {
        Serial.print(" failed, rc=");
        Serial.println(mqtt.state());
        return;
    }
    Serial.println(" OK");

    mqtt.subscribe(TOPIC_NODE_CMD_SUB);
    publishAll();
}

static void publishAll() {
    publishMasterStatus(true);
    publishNodeMeta();
    publishNodeAvailability();
    publishNodeState();
}

static void publishMasterStatus(bool online) {
    StaticJsonDocument<256> doc;
    doc["master_id"] = CONF_MASTER_ID;
    doc["online"] = online;
    doc["wifi"] = (WiFi.status() == WL_CONNECTED);
    doc["mqtt"] = online;
    doc["espnow"] = false;
    doc["fw"] = CONF_FW_VERSION;
    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(TOPIC_MASTER_STATUS, (uint8_t*)buf, strlen(buf), true);
}

static void publishNodeMeta() {
    StaticJsonDocument<768> doc;
    doc["device_id"] = g_node.device_id;
    doc["device_name"] = g_node.device_name;
    doc["device_class"] = "net_zrl";
    doc["power_type"] = "mains";
    doc["fw_version"] = 100;
    doc["caps"] = 8195;
    doc["mac_address"] = g_mac;
    doc["meta_schema_version"] = 1;
    doc["control_mode"] = "cover";
    doc["config_profile"] = "cover_basic";
    doc["reporting_mode"] = "hybrid";
    doc["sensor_mask"] = "XXXXXXXXXX";
    doc["input_mask"] = "XXXXX";
    char buf[768];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(TOPIC_NODE_META, (uint8_t*)buf, strlen(buf), true);
}

static void publishNodeAvailability() {
    StaticJsonDocument<192> doc;
    doc["device_id"] = g_node.device_id;
    doc["availability"] = g_node.available ? "online" : "offline";
    doc["online"] = g_node.available;
    doc["power_type"] = "mains";
    char buf[192];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(TOPIC_NODE_AVAIL, (uint8_t*)buf, strlen(buf), true);
}

static void publishNodeState() {
    StaticJsonDocument<256> doc;
    doc["device_id"] = g_node.device_id;
    doc["relay_1"] = g_node.relay_1;
    doc["relay_2"] = g_node.relay_2;
    doc["cover_mode"] = g_node.cover_mode;
    doc["cover_state"] = g_node.cover_state;
    doc["cover_calibrated"] = g_node.cover_calibrated;
    doc["fault"] = g_node.fault;

    if (g_node.cover_calibrated && g_node.cover_position >= 0) {
        doc["cover_position"] = g_node.cover_position;
    } else {
        doc["cover_position"] = nullptr;
    }

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(TOPIC_NODE_STATE, (uint8_t*)buf, strlen(buf), true);
}

static void publishNodeAck(const char* device_id, const char* request_id,
                           const char* status, int status_code,
                           uint8_t ack_msg_type, const char* source) {
    g_node.ack_seq++;

    char topic[64];
    snprintf(topic, sizeof(topic), "smarthome/device/%s/ack", device_id);

    StaticJsonDocument<256> doc;
    doc["device_id"] = device_id;
    doc["request_id"] = request_id;
    doc["channel"] = "command";
    doc["status"] = status;
    doc["status_code"] = status_code;
    doc["ack_msg_type"] = ack_msg_type;
    doc["ack_seq"] = g_node.ack_seq;
    doc["source"] = source;

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(topic, (uint8_t*)buf, strlen(buf), false);
}

static void onMqttMessage(const char* topic, byte* payload, unsigned int len) {
    const char* p = topic;
    int slashes = 0;
    const char* id_start = nullptr;
    const char* id_end = nullptr;
    while (*p) {
        if (*p == '/') {
            slashes++;
            if (slashes == 2) {
                id_start = p + 1;
            }
            if (slashes == 3) {
                id_end = p;
                break;
            }
        }
        p++;
    }
    if (!id_start || !id_end || id_end <= id_start) {
        return;
    }

    size_t id_len = (size_t)(id_end - id_start);
    char device_id[32];
    if (id_len >= sizeof(device_id)) {
        return;
    }
    memcpy(device_id, id_start, id_len);
    device_id[id_len] = '\0';

    char pstr[MQTT_BUF_SIZE];
    if (len >= sizeof(pstr)) {
        len = sizeof(pstr) - 1;
    }
    memcpy(pstr, payload, len);
    pstr[len] = '\0';

    Serial.print("[cmd] device=");
    Serial.print(device_id);
    Serial.print(" payload=");
    Serial.println(pstr);

    handleCommand(device_id, pstr, len);
}

static void handleCommand(const char* device_id,
                          const char* payload_str, unsigned int len) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload_str, len);
    if (err) {
        Serial.print("[cmd] JSON error: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc.containsKey("request_id") || doc["request_id"].isNull()) {
        Serial.println("[cmd] no request_id - silent drop");
        return;
    }
    const char* request_id = doc["request_id"].as<const char*>();
    const char* command = doc["command"] | "";

    if (strcmp(device_id, g_node.device_id) != 0) {
        publishNodeAck(device_id, request_id,
                       "unknown_device", -6, 7, "master_registry");
        return;
    }

    if (strcmp(command, "get_state") == 0) {
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }

    if (strcmp(command, "open") == 0) {
        g_node.relay_1 = true;
        g_node.relay_2 = false;
        strlcpy(g_node.cover_state, "opening", sizeof(g_node.cover_state));
        if (g_node.cover_calibrated) {
            g_node.cover_position = 100;
            strlcpy(g_node.cover_state, "open", sizeof(g_node.cover_state));
            g_node.relay_1 = false;
        }
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }

    if (strcmp(command, "close") == 0) {
        g_node.relay_1 = false;
        g_node.relay_2 = true;
        strlcpy(g_node.cover_state, "closing", sizeof(g_node.cover_state));
        if (g_node.cover_calibrated) {
            g_node.cover_position = 0;
            strlcpy(g_node.cover_state, "closed", sizeof(g_node.cover_state));
            g_node.relay_2 = false;
        }
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }

    if (strcmp(command, "stop") == 0) {
        g_node.relay_1 = false;
        g_node.relay_2 = false;
        strlcpy(g_node.cover_state, "stopped", sizeof(g_node.cover_state));
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }

    if (strcmp(command, "set_position") == 0) {
        int val = doc["value"] | -1;
        if (!g_node.cover_calibrated && val > 0 && val < 100) {
            publishNodeAck(device_id, request_id,
                           "not_calibrated", -5, 7, "master_validation");
            return;
        }
        if (val < 0 || val > 100) {
            publishNodeAck(device_id, request_id,
                           "invalid_payload", -21, 7, "master_validation");
            return;
        }
        g_node.cover_position = val;
        g_node.relay_1 = false;
        g_node.relay_2 = false;
        if (val == 100) {
            strlcpy(g_node.cover_state, "open", sizeof(g_node.cover_state));
        } else if (val == 0) {
            strlcpy(g_node.cover_state, "closed", sizeof(g_node.cover_state));
        } else {
            strlcpy(g_node.cover_state, "stopped", sizeof(g_node.cover_state));
        }
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }

#ifdef CONF_ENABLE_SIM_HOOKS
    if (strcmp(command, "simulate_calibrated") == 0) {
        bool val = doc["value"] | false;
        g_node.cover_calibrated = val;
        if (val && g_node.cover_position < 0) {
            g_node.cover_position = 0;
        }
        if (!val) {
            g_node.cover_position = -1;
            strlcpy(g_node.cover_state, "stopped", sizeof(g_node.cover_state));
        }
        publishNodeState();
        publishNodeAck(device_id, request_id, "ok", 0, 7, "node_ack");
        return;
    }
#endif

    publishNodeAck(device_id, request_id,
                   "unsupported", -2, 7, "master_validation");
}
