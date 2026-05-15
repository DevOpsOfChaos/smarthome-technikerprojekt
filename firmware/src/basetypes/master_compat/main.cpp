// =============================================================================
// main.cpp – master_compat: Minimaler ESP-NOW-MQTT-Bridge (Stage 2)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/master_compat/main.cpp
//
// Datei-Funktion:
//   Minimaler ESP-NOW-MQTT-Bridge fuer genau einen net_zrl-Node.
//   Uebersetzt MQTT-Kommandos (get_state, open, close, stop,
//   set_position) in ESP-NOW-Protokoll-Nachrichten und sendet
//   Node-Status (STATE, Availability, ACK) zurueck auf MQTT.
//   Nutzt ArduinoJson fuer JSON-Bau/Parsing.
//
// MQTT-Topics:
//   smarthome/master/{MASTER_ID}/status            – Master-Status (retain)
//   smarthome/device/{NODE_ID}/{meta,availability,state,ack}
//   smarthome/device/+/command                      – Eingehende Kommandos
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch, Doxygen-Stil)
//
// Abhaengigkeiten:
//   config.h (WLAN/MQTT-Zugangsdaten – NICHT im Repo!),
//   ArduinoJson, lib/sh_protocol (Protocol.h, DeviceTypes.h)
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "config.h"
#include "../../../lib/sh_protocol/src/Protocol.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// =============================================================================
// KONSTANTEN – Buffer, Intervalle, Timeouts
// =============================================================================

#define MQTT_BUF_SIZE           1024U          // MQTT-Empfangspuffer (Bytes)
#define RECONNECT_DELAY_MS      3000UL         // MQTT-Reconnect-Intervall (ms)
#define WIFI_RETRY_INTERVAL_MS  5000UL         // WLAN-Reconnect-Intervall (ms)
#define COMMAND_TIMEOUT_MS      7000UL         // ACK-Timeout fuer Kommandos (ms)
#define NODE_OFFLINE_TIMEOUT_MS 30000UL        // Offline-Timeout der Node (ms)
#define REQUEST_ID_LEN          96U            // Maximale request_id-Laenge

// MQTT-Topics (statische Strings)
static const char TOPIC_MASTER_STATUS[] =
    "smarthome/master/" CONF_MASTER_ID "/status";
static const char TOPIC_NODE_CMD_SUB[] = "smarthome/device/+/command";

// =============================================================================
// STRUKTUREN – PendingCommand und NetZrlNode-Zustand
// =============================================================================

// PendingCommand – Ausstehendes Kommando (wartet auf ACK von der Node)
struct PendingCommand {
    bool active;                    // true = Pending laeuft
    uint8_t seq;                    // ESP-NOW-Sequenznummer
    uint8_t msg_type;               // Nachrichtentyp (SH_MSG_CMD oder SH_MSG_CFG)
    uint8_t cmd_type;               // Kommando-Typ (SH_CMD_COVER, SH_CMD_STATE_REQUEST)
    uint8_t param1;                 // Parameter 1 (z.B. Cover-Aktion)
    uint8_t param2;                 // Parameter 2 (z.B. Zielposition)
    uint32_t sent_ms;               // Zeitstempel letzter Sendevorgang
    char request_id[REQUEST_ID_LEN]; // request_id aus MQTT (fuer ACK-Routing)
};

// NetZrlNode – Zustand der angemeldeten net_zrl-Node
struct NetZrlNode {
    bool registered_node;           // true = Node registriert
    bool meta_known;                // true = Meta-Daten von HELLO empfangen
    bool state_known;               // true = Mindestens ein STATE empfangen
    bool online;                    // true = Node innerhalb Timeout aktiv
    bool mac_known;                 // true = MAC-Adresse bekannt
    uint8_t mac[6];                 // MAC-Adresse der Node
    uint32_t last_contact_ms;       // Zeitstempel letzter Kontakt
    uint16_t caps;                  // Faehigkeiten (Bitmaske)
    uint16_t fw_version;            // Firmware-Version
    uint8_t power_type;             // Stromversorgung (mains/battery)
    uint8_t meta_schema_version;    // Meta-Daten-Schema-Version
    uint8_t control_mode;           // Steuerungsmodus
    uint8_t config_profile;         // Konfigurationsprofil
    uint8_t reporting_mode;         // Report-Modus
    char device_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    char device_name[SH_DEVICE_NAME_LEN]; // Geraete-Name
    char sensor_mask[SH_SENSOR_MASK_LEN]; // Sensor-Maske
    char input_mask[SH_INPUT_MASK_LEN];   // Input-Maske
    bool relay_1;                   // Relais 1 Zustand
    bool relay_2;                   // Relais 2 Zustand
    bool cover_mode;                // Cover-Modus aktiv
    uint8_t cover_state;            // Cover-State (SH_COVER_STATE_*)
    uint8_t cover_position;         // Cover-Position (0-100, 255=unbekannt)
    bool cover_calibrated;          // Cover kalibriert
    bool fault;                     // Fehlerstatus
    PendingCommand pending;         // Ausstehendes Kommando
};

// Globale Instanz der Bridge (einzelne Node)
static NetZrlNode g_node = {};
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static bool g_wifi_ok = false;
static bool g_espnow_ready = false;
static uint32_t g_last_wifi_attempt_ms = 0;
static uint32_t g_last_mqtt_attempt_ms = 0;
static uint8_t g_next_seq = 1;

// =============================================================================
// HILFSFUNKTIONEN – Texte kopieren, MAC formatieren, Enum-to-Text
// =============================================================================

// copyText – Sicheres Kopieren eines null-terminierten Strings
static void copyText(char* target, size_t target_size, const char* source) {
    if (target == nullptr || target_size == 0U) return;
    if (source == nullptr) {
        target[0] = '\0';
        return;
    }
    strncpy(target, source, target_size - 1U);
    target[target_size - 1U] = '\0';
}

// copyFixedText – Kopiert einen String mit fester Laenge aus einem Protokoll-Puffer
static void copyFixedText(char* target, size_t target_size,
                          const char* source, size_t source_len) {
    if (target == nullptr || target_size == 0U) return;
    size_t len = source_len;
    if (len > (target_size - 1U)) len = target_size - 1U;
    memcpy(target, source, len);
    target[len] = '\0';
}

// macToText – Formatiert eine MAC-Adresse als "AA:BB:CC:DD:EE:FF"
static void macToText(const uint8_t* mac, char* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size == 0U) return;
    if (mac == nullptr || !SmartHome::isValidMac(mac)) {
        copyText(buffer, buffer_size, "00:00:00:00:00:00");
        return;
    }
    char local[18] = {0};
    SmartHome::macToString(mac, local);
    copyText(buffer, buffer_size, local);
}

// helloCaps – Extrahiert die 16-Bit-Faehigkeiten aus dem HELLO-Payload
static uint16_t helloCaps(const SmartHome::HelloPayload& payload) {
    return (uint16_t)(((uint16_t)payload.caps_hi << 8) | payload.caps_lo);
}

// powerTypeText – Wandelt Power-Type in lesbaren String
static const char* powerTypeText(uint8_t power_type) {
    return power_type == SH_POWER_BATTERY ? "battery" : "mains";
}

// controlModeText – Wandelt Control-Mode in lesbaren String
static const char* controlModeText(uint8_t control_mode) {
    switch (control_mode) {
        case SH_CONTROL_MODE_RELAY: return "relay";
        case SH_CONTROL_MODE_RELAY_LIGHT: return "relay_light";
        case SH_CONTROL_MODE_DUAL_RELAY: return "dual_relay";
        case SH_CONTROL_MODE_DUAL_RELAY_LIGHT: return "dual_relay_light";
        case SH_CONTROL_MODE_COVER: return "cover";
        case SH_CONTROL_MODE_NONE:
        default: return "none";
    }
}

// configProfileText – Wandelt Config-Profil in lesbaren String
static const char* configProfileText(uint8_t config_profile) {
    switch (config_profile) {
        case SH_PROFILE_HALL_LIGHT: return "hall_light";
        case SH_PROFILE_KITCHEN_LIGHT: return "kitchen_light";
        case SH_PROFILE_COVER_BASIC: return "cover_basic";
        case SH_PROFILE_NONE:
        default: return "none";
    }
}

// reportingModeText – Wandelt Report-Modus in lesbaren String
static const char* reportingModeText(uint8_t reporting_mode) {
    switch (reporting_mode) {
        case SH_REPORTING_PERIODIC: return "periodic";
        case SH_REPORTING_EVENT_DRIVEN: return "event_driven";
        case SH_REPORTING_HYBRID: return "hybrid";
        case SH_REPORTING_SLEEP_PERIODIC: return "sleep_periodic";
        case SH_REPORTING_SLEEP_EVENT: return "sleep_event";
        default: return "unknown";
    }
}

// coverStateText – Gibt den Cover-State als MQTT-String zurueck
static const char* coverStateText() {
    switch (g_node.cover_state) {
        case SH_COVER_STATE_MOVING_UP: return "opening";
        case SH_COVER_STATE_MOVING_DOWN: return "closing";
        case SH_COVER_STATE_STOPPED:
        default:
            if (g_node.cover_calibrated && g_node.cover_position == 100U) return "open";
            if (g_node.cover_calibrated && g_node.cover_position == 0U) return "closed";
            return "stopped";
    }
}

// ackStatusText – Wandelt ACK-Status-Code in lesbaren String
static const char* ackStatusText(uint8_t status) {
    switch (status) {
        case SH_ACK_OK: return "ok";
        case SH_ACK_REJECTED: return "rejected";
        case SH_ACK_ERROR:
        default: return "error";
    }
}

// =============================================================================
// MQTT / JSON-HELFER – Topic bauen, Status publishen (Meta, State, Availability, Ack)
// =============================================================================

// buildNodeTopic – Baut ein MQTT-Topic fuer eine Node
static void buildNodeTopic(const char* device_id, const char* suffix,
                           char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "smarthome/device/%s/%s",
             device_id != nullptr ? device_id : "unknown", suffix);
}

// publishMasterStatus – Publisht Master-Online-Status auf MQTT (retained)
static void publishMasterStatus(bool online) {
    StaticJsonDocument<256> doc;
    doc["master_id"] = CONF_MASTER_ID;
    doc["online"] = online;
    doc["wifi"] = (WiFi.status() == WL_CONNECTED);
    doc["mqtt"] = online;
    doc["espnow"] = g_espnow_ready;
    doc["fw"] = CONF_FW_VERSION;

    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(TOPIC_MASTER_STATUS, (uint8_t*)payload, strlen(payload), true);
}

// publishNodeMeta – Publisht Node-Metadaten (HELLO-info) auf MQTT (retained)
static void publishNodeMeta() {
    if (!g_node.registered_node || !g_node.meta_known) return;

    char topic[96];
    char mac_text[18];
    buildNodeTopic(g_node.device_id, "meta", topic, sizeof(topic));
    macToText(g_node.mac_known ? g_node.mac : nullptr, mac_text, sizeof(mac_text));

    StaticJsonDocument<768> doc;
    doc["device_id"] = g_node.device_id;
    doc["device_name"] = g_node.device_name;
    doc["device_class"] = "net_zrl";
    doc["power_type"] = powerTypeText(g_node.power_type);
    doc["fw_version"] = g_node.fw_version;
    doc["caps"] = g_node.caps;
    doc["mac_address"] = mac_text;
    doc["meta_schema_version"] = g_node.meta_schema_version;
    doc["control_mode"] = controlModeText(g_node.control_mode);
    doc["config_profile"] = configProfileText(g_node.config_profile);
    doc["reporting_mode"] = reportingModeText(g_node.reporting_mode);
    doc["sensor_mask"] = g_node.sensor_mask;
    doc["input_mask"] = g_node.input_mask;

    char payload[768];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(topic, (uint8_t*)payload, strlen(payload), true);
}

// publishNodeAvailability – Publisht Node-Online/Offline auf MQTT (retained)
static void publishNodeAvailability() {
    if (!g_node.registered_node) return;

    char topic[96];
    buildNodeTopic(g_node.device_id, "availability", topic, sizeof(topic));

    StaticJsonDocument<192> doc;
    doc["device_id"] = g_node.device_id;
    doc["availability"] = g_node.online ? "online" : "offline";
    doc["online"] = g_node.online;
    doc["power_type"] = powerTypeText(g_node.power_type);

    char payload[192];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(topic, (uint8_t*)payload, strlen(payload), true);
}

// publishNodeState – Publisht Cover-Zustand der Node auf MQTT (retained)
static void publishNodeState() {
    if (!g_node.registered_node || !g_node.state_known) return;

    char topic[96];
    buildNodeTopic(g_node.device_id, "state", topic, sizeof(topic));

    StaticJsonDocument<256> doc;
    doc["device_id"] = g_node.device_id;
    doc["relay_1"] = g_node.relay_1;
    doc["relay_2"] = g_node.relay_2;
    doc["cover_mode"] = g_node.cover_mode;
    doc["cover_state"] = coverStateText();
    doc["cover_calibrated"] = g_node.cover_calibrated;
    doc["fault"] = g_node.fault;

    // Position nur senden wenn kalibriert und bekannt
    if (g_node.cover_calibrated && g_node.cover_position <= 100U) {
        doc["cover_position"] = g_node.cover_position;
    } else {
        doc["cover_position"] = nullptr;
    }

    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(topic, (uint8_t*)payload, strlen(payload), true);
}

// publishNodeAckById – Publisht eine ACK-Antwort an eine beliebige device_id
static void publishNodeAckById(const char* device_id, const char* request_id,
                               const char* status, int status_code,
                               uint8_t ack_msg_type, uint8_t ack_seq,
                               const char* source) {
    char topic[96];
    buildNodeTopic(device_id, "ack", topic, sizeof(topic));

    StaticJsonDocument<256> doc;
    doc["device_id"] = device_id;
    doc["request_id"] = request_id != nullptr ? request_id : "";
    doc["channel"] = "command";
    doc["status"] = status;
    doc["status_code"] = status_code;
    doc["ack_msg_type"] = ack_msg_type;
    doc["ack_seq"] = ack_seq;
    doc["source"] = source;

    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(topic, (uint8_t*)payload, strlen(payload), false);
}

// publishNodeAck – Publisht ACK an die bekannte Node
static void publishNodeAck(const char* request_id, const char* status,
                           int status_code, uint8_t ack_msg_type,
                           uint8_t ack_seq, const char* source) {
    publishNodeAckById(g_node.device_id, request_id, status, status_code,
                       ack_msg_type, ack_seq, source);
}

// publishAllKnown – Publisht beim MQTT-Reconnect alle bekannten Zustaende
static void publishAllKnown() {
    publishMasterStatus(true);
    publishNodeMeta();
    publishNodeAvailability();
    publishNodeState();
}

// =============================================================================
// ESP-NOW – Peer, Paket senden, Kommando senden
// =============================================================================

// ensurePeer – Registriert eine MAC als ESP-NOW-Peer (falls nicht vorhanden)
static bool ensurePeer(const uint8_t* mac) {
    if (!SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = (uint8_t)(WiFi.status() == WL_CONNECTED ? WiFi.channel() : 0);
    peer.encrypt = false;

    return esp_now_add_peer(&peer) == ESP_OK;
}

// sendPacketWithOptions – Zentrale ESP-NOW-Sendefunktion mit Flags und fester Sequenz
static bool sendPacketWithOptions(const uint8_t* dest_mac, uint8_t msg_type,
                                  const void* payload, size_t payload_len,
                                  uint8_t flags, bool fixed_seq, uint8_t seq,
                                  uint8_t* used_seq) {
    if (!g_espnow_ready || !SmartHome::isValidMac(dest_mac) ||
        payload_len > SH_MAX_PAYLOAD_BYTES) return false;
    if (!ensurePeer(dest_mac)) return false;

    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t effective_seq = fixed_seq ? seq : g_next_seq++;
    SmartHome::fillHeader(header, msg_type, effective_seq, flags,
                          (uint16_t)payload_len);

    if (payload_len > 0U && payload != nullptr) {
        memcpy(buffer + SH_HEADER_SIZE, payload, payload_len);
    }

    SmartHome::finalizePacketCrc(header, buffer + SH_HEADER_SIZE);
    memcpy(buffer, &header, sizeof(header));

    if (used_seq != nullptr) {
        *used_seq = effective_seq;
    }

    return esp_now_send(dest_mac, buffer, SH_HEADER_SIZE + payload_len) == ESP_OK;
}

// sendPacket – Vereinfachte Sendefunktion (ohne Flags, ohne feste Seq)
static bool sendPacket(const uint8_t* dest_mac, uint8_t msg_type,
                       const void* payload, size_t payload_len) {
    return sendPacketWithOptions(dest_mac, msg_type, payload, payload_len,
                                 0U, false, 0U, nullptr);
}

// sendCommand – Sendet ein Kommando an die Node und registriert Pending
static bool sendCommand(uint8_t cmd_type, uint8_t param1, uint8_t param2,
                        const char* request_id) {
    if (!g_node.registered_node || !g_node.mac_known) return false;

    SmartHome::CmdPayload payload = {};
    payload.cmd_type = cmd_type;
    payload.param1 = param1;
    payload.param2 = param2;

    uint8_t seq = 0U;
    if (!sendPacketWithOptions(g_node.mac, SH_MSG_CMD, &payload, sizeof(payload),
                               SH_FLAG_ACK_REQUEST, false, 0U, &seq)) {
        return false;
    }

    // Pending registrieren (erwartet ACK innerhalb COMMAND_TIMEOUT_MS)
    g_node.pending = {};
    g_node.pending.active = true;
    g_node.pending.seq = seq;
    g_node.pending.msg_type = SH_MSG_CMD;
    g_node.pending.cmd_type = cmd_type;
    g_node.pending.param1 = param1;
    g_node.pending.param2 = param2;
    g_node.pending.sent_ms = millis();
    copyText(g_node.pending.request_id, sizeof(g_node.pending.request_id), request_id);
    return true;
}

// requestFreshState – Fordert aktuellen STATE von der Node an
static void requestFreshState() {
    if (!g_node.registered_node || !g_node.mac_known) return;

    SmartHome::CmdPayload payload = {};
    payload.cmd_type = SH_CMD_STATE_REQUEST;
    sendPacket(g_node.mac, SH_MSG_CMD, &payload, sizeof(payload));
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG (ESP-NOW) – HELLO, HEARTBEAT, STATE, ACK
// =============================================================================

// refreshNodeContact – Aktualisiert Kontaktzeit und Online-Status der Node
static void refreshNodeContact(const uint8_t* sender_mac) {
    const bool was_offline = !g_node.online;
    g_node.last_contact_ms = millis();
    g_node.online = true;

    // MAC-Adresse aktualisieren wenn vom Sender mitgeliefert
    if (sender_mac != nullptr && SmartHome::isValidMac(sender_mac)) {
        const bool mac_changed = !g_node.mac_known ||
                                 memcmp(g_node.mac, sender_mac, 6) != 0;
        memcpy(g_node.mac, sender_mac, 6);
        g_node.mac_known = true;
        if (mac_changed) {
            ensurePeer(g_node.mac);
        }
    }

    // Bei Wieder-Online: Availability publishen
    if (was_offline && mqtt.connected()) {
        publishNodeAvailability();
    }
}

// sendHelloAck – Sendet HELLO_ACK an die Node
static void sendHelloAck(const uint8_t* dest_mac, uint8_t ack_status) {
    SmartHome::HelloAckPayload payload = {};
    payload.channel = (uint8_t)(WiFi.status() == WL_CONNECTED ? WiFi.channel() : 0);
    payload.ack_status = ack_status;
    sendPacket(dest_mac, SH_MSG_HELLO_ACK, &payload, sizeof(payload));
}

// isKnownDeviceId – Prueft ob eine device_id der registrierten Node entspricht
static bool isKnownDeviceId(const char* device_id) {
    return g_node.registered_node &&
           strncmp(g_node.device_id, device_id, SH_DEVICE_ID_LEN) == 0;
}

// handleHello – Verarbeitet eingehendes HELLO (Node-Registrierung)
static void handleHello(const uint8_t* sender_mac,
                        const SmartHome::HelloPayload& payload) {
    char device_id[SH_DEVICE_ID_LEN];
    char device_name[SH_DEVICE_NAME_LEN];
    char sensor_mask[SH_SENSOR_MASK_LEN];
    char input_mask[SH_INPUT_MASK_LEN];

    copyFixedText(device_id, sizeof(device_id), payload.device_id,
                  sizeof(payload.device_id));
    copyFixedText(device_name, sizeof(device_name), payload.device_name,
                  sizeof(payload.device_name));
    copyFixedText(sensor_mask, sizeof(sensor_mask), payload.sensor_mask,
                  sizeof(payload.sensor_mask));
    copyFixedText(input_mask, sizeof(input_mask), payload.input_mask,
                  sizeof(payload.input_mask));

    // HELLO ablehnen wenn: ungueltige device_id, falsche Klasse, ungueltiger Power-Type
    if (!SmartHome::isValidDeviceId(device_id) ||
        payload.device_class != SH_CLASS_NET_ZRL ||
        (payload.power_type != SH_POWER_MAINS &&
         payload.power_type != SH_POWER_BATTERY)) {
        sendHelloAck(sender_mac, SH_ACK_REJECTED);
        return;
    }

    // Bestehende Node mit anderer device_id blocken (Single-Node-Bridge)
    if (g_node.registered_node && !isKnownDeviceId(device_id)) {
        sendHelloAck(sender_mac, SH_ACK_REJECTED);
        return;
    }

    g_node.registered_node = true;
    g_node.meta_known = true;
    copyText(g_node.device_id, sizeof(g_node.device_id), device_id);
    copyText(g_node.device_name, sizeof(g_node.device_name), device_name);
    copyText(g_node.sensor_mask, sizeof(g_node.sensor_mask), sensor_mask);
    copyText(g_node.input_mask, sizeof(g_node.input_mask), input_mask);
    g_node.caps = helloCaps(payload);
    g_node.fw_version = payload.fw_version;
    g_node.power_type = payload.power_type;
    g_node.meta_schema_version = payload.meta_schema_version;
    g_node.control_mode = payload.control_mode;
    g_node.config_profile = payload.config_profile;
    g_node.reporting_mode = payload.reporting_mode;

    refreshNodeContact(sender_mac);
    publishNodeMeta();
    publishNodeAvailability();
    if (g_node.state_known) {
        publishNodeState();
    }

    sendHelloAck(sender_mac, SH_ACK_OK);
    requestFreshState();
}

// handleHeartbeat – Verarbeitet eingehenden HEARTBEAT
static void handleHeartbeat(const uint8_t* sender_mac,
                            const SmartHome::HeartbeatPayload& payload) {
    char node_id[SH_DEVICE_ID_LEN];
    copyFixedText(node_id, sizeof(node_id), payload.node_id,
                  sizeof(payload.node_id));
    if (!isKnownDeviceId(node_id)) return;

    refreshNodeContact(sender_mac);
    publishNodeAvailability();
}

// handleZrlState – Aktualisiert den Cover-Zustand aus einem STATE-Payload
static void handleZrlState(const uint8_t* sender_mac, const char* node_id,
                           const SmartHome::ZrlStateReportPayload& state) {
    if (!isKnownDeviceId(node_id)) return;

    refreshNodeContact(sender_mac);
    g_node.relay_1 = (state.relay_1 != 0U);
    g_node.relay_2 = (state.relay_2 != 0U);
    g_node.cover_mode = (state.cover_mode != 0U);
    g_node.cover_state = state.cover_state;
    g_node.cover_position = state.cover_position;
    g_node.cover_calibrated = (state.cover_calibrated != 0U);
    g_node.fault = (state.fault != 0U);
    g_node.state_known = true;

    publishNodeState();
    publishNodeAvailability();
}

// handleStateReport – Verarbeitet STATE (unterstuetzt 2 Payload-Formate)
static void handleStateReport(const uint8_t* sender_mac,
                              const uint8_t* payload,
                              uint16_t payload_len) {
    if (payload == nullptr || payload_len < SH_DEVICE_ID_LEN) return;

    char node_id[SH_DEVICE_ID_LEN];
    copyFixedText(node_id, sizeof(node_id),
                  reinterpret_cast<const char*>(payload), SH_DEVICE_ID_LEN);

    // Standard STATE-Payload (kleiner, ohne Config-Werte)
    if (payload_len == sizeof(SmartHome::ZrlStateReportPayload)) {
        handleZrlState(sender_mac, node_id,
                       *reinterpret_cast<const SmartHome::ZrlStateReportPayload*>(payload));
        return;
    }

    // Config-STATE-Payload (groesser, mit report_interval_s)
    if (payload_len == sizeof(SmartHome::ZrlConfigStateReportPayload)) {
        const SmartHome::ZrlConfigStateReportPayload& state =
            *reinterpret_cast<const SmartHome::ZrlConfigStateReportPayload*>(payload);
        SmartHome::ZrlStateReportPayload live_state = {};
        copyFixedText(live_state.node_id, sizeof(live_state.node_id),
                      state.node_id, sizeof(state.node_id));
        live_state.relay_1 = state.relay_1;
        live_state.relay_2 = state.relay_2;
        live_state.cover_mode = state.cover_mode;
        live_state.cover_state = state.cover_state;
        live_state.cover_position = state.cover_position;
        live_state.cover_calibrated = state.cover_calibrated;
        live_state.fault = state.fault;
        handleZrlState(sender_mac, node_id, live_state);
    }
}

// handleAck – Verarbeitet ACK (meldet Kommando-Bestaetigung auf MQTT)
static void handleAck(const uint8_t* sender_mac,
                      const SmartHome::AckPayload& payload) {
    // Nur ACK von der bekannten Node akzeptieren
    if (!g_node.registered_node || !g_node.mac_known ||
        memcmp(g_node.mac, sender_mac, 6) != 0) return;
    refreshNodeContact(sender_mac);

    // Prueft ob Pending auf diese ACK-Sequenz passt
    if (!g_node.pending.active ||
        payload.ack_msg_type != g_node.pending.msg_type ||
        payload.ack_seq != g_node.pending.seq) return;

    publishNodeAck(g_node.pending.request_id, ackStatusText(payload.status),
                   (int)payload.status, payload.ack_msg_type,
                   payload.ack_seq, "node_ack");
    g_node.pending = {};
}

// =============================================================================
// ESP-NOW – Empfangsverarbeitung, Callbacks
// =============================================================================

// handleEspNowPacket – CRC-Pruefung + Dispatch an Handler (switch/msg_type)
static void handleEspNowPacket(const uint8_t* sender_mac,
                                const uint8_t* data, int length) {
    if (sender_mac == nullptr || data == nullptr ||
        length < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(data, (size_t)length)) return;

    const SmartHome::MsgHeader* header =
        reinterpret_cast<const SmartHome::MsgHeader*>(data);
    const uint8_t* payload = data + SH_HEADER_SIZE;

    switch (header->msg_type) {
        case SH_MSG_HELLO:
            // HELLO: Node anmelden oder ablehnen
            if (header->payload_len == sizeof(SmartHome::HelloPayload)) {
                handleHello(sender_mac,
                            *reinterpret_cast<const SmartHome::HelloPayload*>(payload));
            }
            break;
        case SH_MSG_HEARTBEAT:
            // HEARTBEAT: Kontaktzeit aktualisieren
            if (header->payload_len == sizeof(SmartHome::HeartbeatPayload)) {
                handleHeartbeat(sender_mac,
                                *reinterpret_cast<const SmartHome::HeartbeatPayload*>(payload));
            }
            break;
        case SH_MSG_STATE:
            // STATE: Cover-Zustand verarbeiten (2 unterstuetzte Formate)
            handleStateReport(sender_mac, payload, header->payload_len);
            break;
        case SH_MSG_ACK:
            // ACK: Kommando-Bestaetigung auswerten
            if (header->payload_len == sizeof(SmartHome::AckPayload)) {
                handleAck(sender_mac,
                          *reinterpret_cast<const SmartHome::AckPayload*>(payload));
            }
            break;
        default:
            break;
    }
}

// ESP-NOW Recv-Callback (Core v3 und v2)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onEspNowReceive(const esp_now_recv_info_t* info,
                            const uint8_t* data, int length) {
    if (info == nullptr) return;
    handleEspNowPacket(info->src_addr, data, length);
}
#else
static void onEspNowReceive(const uint8_t* sender_mac,
                            const uint8_t* data, int length) {
    handleEspNowPacket(sender_mac, data, length);
}
#endif

// ESP-NOW Sent-Callback (leer, nur Callback-Registrierung)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void onEspNowSent(const esp_now_send_info_t* info,
                         esp_now_send_status_t status) {
    (void)info;
    (void)status;
}
#else
static void onEspNowSent(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status;
}
#endif

// =============================================================================
// JSON-HILFE – tryGetLong (ArduinoJson-basiert)
// =============================================================================

// tryGetLong – Holt einen Zahlenwert per Key aus einem JsonDocument
static bool tryGetLong(const JsonDocument& doc, const char* key, long* value) {
    if (!doc.containsKey(key) || value == nullptr) return false;
    JsonVariantConst variant = doc[key];
    if (!variant.is<long>() && !variant.is<int>() && !variant.is<unsigned int>()) return false;
    *value = variant.as<long>();
    return true;
}

// =============================================================================
// MQTT-COMMAND-HANDLER – get_state, open, close, stop, set_position
// =============================================================================

// handleCommand – Verarbeitet ein eingehendes MQTT-Kommando
static void handleCommand(const char* device_id, const char* payload_str,
                          unsigned int len) {
    StaticJsonDocument<512> doc;
    // JSON parsen
    if (deserializeJson(doc, payload_str, len) != DeserializationError::Ok) return;

    // request_id ist PFLICHT (dient der ACK-Zuordnung)
    if (!doc.containsKey("request_id") || doc["request_id"].isNull()) return;
    const char* request_id = doc["request_id"].as<const char*>();
    const char* command = doc["command"] | "";

    // Prueft ob Node bekannt ist
    if (!g_node.registered_node || !isKnownDeviceId(device_id)) {
        publishNodeAckById(device_id, request_id, "unknown_device", -6,
                           SH_MSG_CMD, 0U, "master_registry");
        return;
    }

    // Prueft ob bereits ein Pending-Kommando aktiv ist (verhindert Doppel-Kommandos)
    if (g_node.pending.active) {
        publishNodeAck(request_id, "busy", -2, g_node.pending.msg_type,
                       g_node.pending.seq, "master_busy");
        return;
    }

    // Prueft ob Funk und MAC bekannt sind
    if (!g_espnow_ready || !g_node.mac_known) {
        publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                       "master_send");
        return;
    }

    // get_state – Zustandsabfrage (ohne Status-Aenderung)
    if (strcmp(command, "get_state") == 0) {
        if (!sendCommand(SH_CMD_STATE_REQUEST, 0U, 0U, request_id)) {
            publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                           "master_send");
        }
        return;
    }

    // open – Cover ganz oeffnen (Position 100%)
    if (strcmp(command, "open") == 0) {
        if (!sendCommand(SH_CMD_COVER, SH_COVER_CMD_OPEN, 0U, request_id)) {
            publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                           "master_send");
        }
        return;
    }

    // close – Cover ganz schliessen (Position 0%)
    if (strcmp(command, "close") == 0) {
        if (!sendCommand(SH_CMD_COVER, SH_COVER_CMD_CLOSE, 0U, request_id)) {
            publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                           "master_send");
        }
        return;
    }

    // stop – Cover anhalten
    if (strcmp(command, "stop") == 0) {
        if (!sendCommand(SH_CMD_COVER, SH_COVER_CMD_STOP, 0U, request_id)) {
            publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                           "master_send");
        }
        return;
    }

    // set_position – Cover in Teil-Position fahren (0-100%)
    if (strcmp(command, "set_position") == 0) {
        long position = -1L;
        if (!tryGetLong(doc, "value", &position)) {
            (void)tryGetLong(doc, "position", &position);
        }
        if (position < 0L || position > 100L) {
            publishNodeAck(request_id, "invalid_payload", -21,
                           SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        // Ohne Kalibrierung nur Endlagen (0/100) erlaubt
        if (g_node.state_known && !g_node.cover_calibrated &&
            position > 0L && position < 100L) {
            publishNodeAck(request_id, "not_calibrated", -5,
                           SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (!sendCommand(SH_CMD_COVER, SH_COVER_CMD_SET_POSITION,
                         (uint8_t)position, request_id)) {
            publishNodeAck(request_id, "send_failed", -4, SH_MSG_CMD, 0U,
                           "master_send");
        }
        return;
    }

    // Unbekanntes Kommando: ACK mit "unsupported"
    publishNodeAck(request_id, "unsupported", -2, SH_MSG_CMD, 0U,
                   "master_validation");
}

// onMqttMessage – MQTT-Callback: Parst Topic, extrahiert device_id, ruft handleCommand auf
static void onMqttMessage(char* topic, byte* payload, unsigned int len) {
    if (topic == nullptr || payload == nullptr) return;

    // Nur Topics im Format smarthome/device/{id}/command akzeptieren
    const char* prefix = "smarthome/device/";
    if (strncmp(topic, prefix, strlen(prefix)) != 0) return;

    const char* id_start = topic + strlen(prefix);
    const char* suffix = strchr(id_start, '/');
    if (suffix == nullptr || strcmp(suffix, "/command") != 0) return;

    size_t id_len = (size_t)(suffix - id_start);
    if (id_len == 0U || id_len >= SH_DEVICE_ID_LEN) return;

    char device_id[SH_DEVICE_ID_LEN];
    memcpy(device_id, id_start, id_len);
    device_id[id_len] = '\0';

    char json[MQTT_BUF_SIZE];
    if (len >= sizeof(json)) len = sizeof(json) - 1U;
    memcpy(json, payload, len);
    json[len] = '\0';

    handleCommand(device_id, json, len);
}

// =============================================================================
// KONNEKTIVITAET – WLAN, ESP-NOW, MQTT
// =============================================================================

// wifiConnect – Synchroner WLAN-Connect mit max. 40 Versuchen (20s)
static void wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(CONF_WIFI_SSID, CONF_WIFI_PASS);
    g_last_wifi_attempt_ms = millis();
    Serial.print("[wifi] connecting");

    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40U) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    g_wifi_ok = (WiFi.status() == WL_CONNECTED);
    if (g_wifi_ok) {
        Serial.print("[wifi] IP=");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[wifi] connection failed - will retry in loop");
    }
}

// initEspNowIfNeeded – Startet ESP-NOW wenn WLAN verbunden ist
static void initEspNowIfNeeded() {
    if (g_espnow_ready || WiFi.status() != WL_CONNECTED) return;

    if (esp_now_init() != ESP_OK) {
        g_espnow_ready = false;
        return;
    }

    esp_now_register_send_cb(onEspNowSent);
    esp_now_register_recv_cb(onEspNowReceive);
    g_espnow_ready = true;
}

// mqttConnect – Verbindet zu MQTT und subscribed auf Node-Commands
static void mqttConnect() {
    StaticJsonDocument<256> lwt_doc;
    lwt_doc["master_id"] = CONF_MASTER_ID;
    lwt_doc["online"] = false;
    lwt_doc["wifi"] = false;
    lwt_doc["mqtt"] = false;
    lwt_doc["espnow"] = false;
    lwt_doc["fw"] = CONF_FW_VERSION;

    char lwt_payload[256];
    serializeJson(lwt_doc, lwt_payload, sizeof(lwt_payload));

    const char* user = strlen(CONF_MQTT_USER) > 0 ? CONF_MQTT_USER : nullptr;
    const char* pass = strlen(CONF_MQTT_PASS) > 0 ? CONF_MQTT_PASS : nullptr;

    if (!mqtt.connect(CONF_MASTER_ID, user, pass, TOPIC_MASTER_STATUS, 1, true,
                      lwt_payload)) {
        return;
    }

    mqtt.subscribe(TOPIC_NODE_CMD_SUB);
    publishAllKnown();
}

// ensureConnectivity – Erhaelt WLAN/ESP-NOW/MQTT-Konnektivitaet (wird zyklisch aufgerufen)
static void ensureConnectivity() {
    const bool wifi_now = (WiFi.status() == WL_CONNECTED);
    if (!wifi_now && (millis() - g_last_wifi_attempt_ms) >= WIFI_RETRY_INTERVAL_MS) {
        WiFi.disconnect();
        WiFi.begin(CONF_WIFI_SSID, CONF_WIFI_PASS);
        g_last_wifi_attempt_ms = millis();
    }
    g_wifi_ok = wifi_now;

    if (!g_wifi_ok) return;

    initEspNowIfNeeded();

    if (!mqtt.connected() &&
        (millis() - g_last_mqtt_attempt_ms) >= RECONNECT_DELAY_MS) {
        g_last_mqtt_attempt_ms = millis();
        mqttConnect();
    }
}

// =============================================================================
// TIMEOUT-PRUEFUNGEN – Pending-Timeout und Offline-Timeout
// =============================================================================

// checkPendingTimeout – Prueft ob ein ausstehendes Kommando getimeoutet ist
static void checkPendingTimeout() {
    if (!g_node.pending.active) return;
    if ((millis() - g_node.pending.sent_ms) < COMMAND_TIMEOUT_MS) return;

    publishNodeAck(g_node.pending.request_id, "timeout", (int)SH_ERROR_ACK_TIMEOUT,
                   g_node.pending.msg_type, g_node.pending.seq, "master_timeout");
    g_node.pending = {};
}

// checkNodeOffline – Prueft ob die Node als offline markiert werden muss
static void checkNodeOffline() {
    if (!g_node.registered_node || !g_node.online) return;
    if ((millis() - g_node.last_contact_ms) <= NODE_OFFLINE_TIMEOUT_MS) return;

    g_node.online = false;
    if (mqtt.connected()) {
        publishNodeAvailability();
    }
}

// =============================================================================
// ARDUINO – setup() und loop()
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[master_compat] boot");

    // Node-Defaults initialisieren
    copyText(g_node.sensor_mask, sizeof(g_node.sensor_mask), "XXXXXXXXXX");
    copyText(g_node.input_mask, sizeof(g_node.input_mask), "XXXXX");

    // MQTT konfigurieren
    mqtt.setServer(CONF_MQTT_HOST, CONF_MQTT_PORT);
    mqtt.setBufferSize(MQTT_BUF_SIZE);
    mqtt.setCallback(onMqttMessage);

    wifiConnect();
    initEspNowIfNeeded();
    mqttConnect();
}

void loop() {
    ensureConnectivity();

    if (mqtt.connected()) {
        mqtt.loop();
    }

    checkPendingTimeout();
    checkNodeOffline();
    delay(10);
}
