/*
====================================================================
 Projekt   : SmartHome ESP32
 Geraet    : Master (ESP32-C3)
 Datei     : main.cpp
 Version   : 0.4.0
 Stand     : 2026-04-08

 Funktion:
 Dynamischer Master als Funk- und MQTT-Bruecke.

 Ziel dieses Stands:
 - HELLO / HELLO_ACK / HEARTBEAT / STATE / EVENT / ACK
 - dynamische Node-Registry statt fester Geraeteliste
 - einfache Availability auf Basis der Registry
 - set_relay / get_state / minimales set_config
 - Cover-Befehle open / close / stop / set_position
 - Pending-/ACK-Modell pro Geraet

 Wichtige Architekturgrenze:
 Der Master ist Bruecke und einfache Projektion,
 aber kein halber Server und kein Dashboard-Generator.
====================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "AppConfig.h"
#include "PinConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../include/DebugConfig.h"
#include "../../../lib/sh_protocol/src/Protocol.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_storage/src/ShStorage.h"

#if __has_include("../../../include/Secrets.h")
  #include "../../../include/Secrets.h"
#else
  #warning "Keine Secrets.h gefunden. Bitte aus Secrets.example.h erstellen."
  #define WIFI_SSID         "KEIN_SSID"
  #define WIFI_PASSWORD     "KEIN_PASSWORT"
  #define MQTT_HOST         "127.0.0.1"
  #define MQTT_PORT         1883
  #define MQTT_USER         "mqtt_user"
  #define MQTT_PASSWORD     "KEIN_MQTT_PASSWORT"
#endif

#ifndef MQTT_USER
  #define MQTT_USER ""
#endif

#ifndef MQTT_PASSWORD
  #define MQTT_PASSWORD ""
#endif

namespace {

constexpr bool DEBUG_LOKAL_AKTIV = DEVICE_DEBUG_AKTIV && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "MASTER";
constexpr char DATEI_VERSION[] = "0.4.0";
constexpr char MQTT_TOPIC_COMMAND_SUB[] = "smarthome/device/+/command";
constexpr size_t REQUEST_ID_LEN = 96U;
constexpr long CFG_REPORT_INTERVAL_MIN = (long)SmartHome::ShStorage::SH_STORED_REPORT_INTERVAL_MIN_S;
constexpr long CFG_REPORT_INTERVAL_MAX = (long)SmartHome::ShStorage::SH_STORED_REPORT_INTERVAL_MAX_S;
constexpr uint32_t NET_SEN_PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint32_t NET_SEN_GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint16_t NET_SEN_AIR_METRIC_UNGUELTIG = 0xFFFFU;
constexpr uint8_t BATTERY_PCT_UNGUELTIG = 0xFFU;
constexpr uint16_t BATTERY_MV_UNGUELTIG = 0U;
constexpr uint8_t WINDOW_STATE_UNGUELTIG = 0xFFU;
constexpr uint16_t RAIN_RAW_UNGUELTIG = 0xFFFFU;
constexpr int STATUS_CODE_NOT_CALIBRATED = -5;
constexpr int STATUS_CODE_UNKNOWN_DEVICE = -6;
constexpr int STATUS_CODE_REGISTRY_FULL = -7;

struct PendingCmdRequest {
    bool aktiv;
    uint8_t seq;
    uint8_t retries;
    uint8_t cmd_type;
    uint8_t param1;
    uint8_t param2;
    unsigned long letztes_senden_ms;
    char request_id[REQUEST_ID_LEN];
    char command_channel[24];
};

struct PendingConfigRequest {
    bool aktiv;
    uint8_t seq;
    uint8_t retries;
    uint8_t param_id;
    uint16_t value;
    unsigned long letztes_senden_ms;
    char request_id[REQUEST_ID_LEN];
    char command_channel[24];
};

struct NodeRuntime {
    bool belegt;
    bool meta_bekannt;
    bool online;
    bool state_bekannt;
    bool mac_bekannt;
    bool fault;
    bool relay_1;
    bool relay_2;
    bool cover_mode;
    bool cover_calibrated;
    uint8_t cover_state;
    uint8_t cover_position;
    int16_t temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    bool motion;
    uint8_t battery_pct;
    uint16_t battery_mv;
    uint8_t window_open;
    uint16_t rain_raw;
    uint8_t button_flags;
    uint32_t uptime_s;
    uint16_t caps;
    uint16_t fw_version;
    uint8_t device_class;
    uint8_t power_type;
    uint8_t meta_schema_version;
    uint8_t control_mode;
    uint8_t config_profile;
    uint8_t reporting_mode;
    char device_id[SH_DEVICE_ID_LEN];
    char sensor_mask[SH_SENSOR_MASK_LEN];
    char input_mask[SH_INPUT_MASK_LEN];
    char device_name[SH_DEVICE_NAME_LEN];
    uint8_t mac[6];
    unsigned long letzter_kontakt_ms;
    PendingCmdRequest pending_cmd;
    PendingConfigRequest pending_cfg;
};

struct MasterState {
    bool wlan_verbunden;
    bool mqtt_verbunden;
    bool espnow_bereit;
    unsigned long letzter_wlan_versuch_ms;
    unsigned long letzter_mqtt_versuch_ms;
    uint8_t naechste_seq;
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
IPAddress mqttBrokerIp;
bool mqttBrokerNutzeDirekteIp = false;
MasterState masterStatus = {};
NodeRuntime nodeStates[MAX_DYNAMIC_NODES] = {};

void logf(const char* level, const char* format, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

void copyText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    if (!source) {
        target[0] = '\0';
        return;
    }
    strncpy(target, source, targetSize - 1U);
    target[targetSize - 1U] = '\0';
}

void macText(const uint8_t* mac, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0U) return;
    if (!mac) {
        copyText(buffer, bufferSize, "unbekannt");
        return;
    }
    char local[18] = {0};
    SmartHome::macToString(mac, local);
    copyText(buffer, bufferSize, local);
}

const char* mqttBrokerTypText() {
    return mqttBrokerNutzeDirekteIp ? "ip" : "host";
}

const char* deviceClassText(uint8_t deviceClass) {
    switch (deviceClass) {
        case SH_CLASS_NET_ERL: return "net_erl";
        case SH_CLASS_NET_ZRL: return "net_zrl";
        case SH_CLASS_NET_SEN: return "net_sen";
        case SH_CLASS_BAT_SEN: return "bat_sen";
        case SH_CLASS_MASTER:  return "master";
        default: return "unknown";
    }
}

const char* powerTypeText(uint8_t powerType) {
    return powerType == SH_POWER_BATTERY ? "battery" : "mains";
}

const char* controlModeText(uint8_t controlMode) {
    switch (controlMode) {
        case SH_CONTROL_MODE_RELAY: return "relay";
        case SH_CONTROL_MODE_RELAY_LIGHT: return "relay_light";
        case SH_CONTROL_MODE_DUAL_RELAY: return "dual_relay";
        case SH_CONTROL_MODE_DUAL_RELAY_LIGHT: return "dual_relay_light";
        case SH_CONTROL_MODE_COVER: return "cover";
        case SH_CONTROL_MODE_NONE:
        default: return "none";
    }
}

const char* configProfileText(uint8_t configProfile) {
    switch (configProfile) {
        case SH_PROFILE_HALL_LIGHT: return "hall_light";
        case SH_PROFILE_KITCHEN_LIGHT: return "kitchen_light";
        case SH_PROFILE_COVER_BASIC: return "cover_basic";
        case SH_PROFILE_NONE:
        default: return "none";
    }
}

const char* reportingModeText(uint8_t reportingMode) {
    switch (reportingMode) {
        case SH_REPORTING_PERIODIC: return "periodic";
        case SH_REPORTING_EVENT_DRIVEN: return "event_driven";
        case SH_REPORTING_HYBRID: return "hybrid";
        case SH_REPORTING_SLEEP_PERIODIC: return "sleep_periodic";
        case SH_REPORTING_SLEEP_EVENT: return "sleep_event";
        default: return "unknown";
    }
}

const char* coverStateText(uint8_t coverStateCode, bool calibrated, uint8_t position) {
    switch (coverStateCode) {
        case SH_COVER_STATE_MOVING_UP: return "opening";
        case SH_COVER_STATE_MOVING_DOWN: return "closing";
        case SH_COVER_STATE_STOPPED:
        default:
            if (calibrated && position == 100U) return "open";
            if (calibrated && position == 0U) return "closed";
            return "stopped";
    }
}

bool istDeviceClassGueltig(uint8_t deviceClass) {
    switch (deviceClass) {
        case SH_CLASS_NET_ERL:
        case SH_CLASS_NET_ZRL:
        case SH_CLASS_NET_SEN:
        case SH_CLASS_BAT_SEN:
            return true;
        default:
            return false;
    }
}

bool istPowerTypeGueltig(uint8_t powerType) {
    return powerType == SH_POWER_MAINS || powerType == SH_POWER_BATTERY;
}

unsigned long offlineTimeoutMsForPowerType(uint8_t powerType) {
    return powerType == SH_POWER_BATTERY ? BATTERY_NODE_OFFLINE_TIMEOUT_MS : NODE_OFFLINE_TIMEOUT_MS;
}

void initialisiereNodeSlot(NodeRuntime& node) {
    node = {};
    node.cover_state = SH_COVER_STATE_STOPPED;
    node.cover_position = 0U;
    node.temp_01c = INT16_MIN;
    node.hum_01pct = 0xFFFFU;
    node.lux = 0xFFFFU;
    node.pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    node.gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
    node.aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.battery_pct = BATTERY_PCT_UNGUELTIG;
    node.battery_mv = BATTERY_MV_UNGUELTIG;
    node.window_open = WINDOW_STATE_UNGUELTIG;
    node.rain_raw = RAIN_RAW_UNGUELTIG;
    node.meta_schema_version = SH_META_SCHEMA_VERSION_CURRENT;
    node.control_mode = SH_CONTROL_MODE_NONE;
    node.config_profile = SH_PROFILE_NONE;
    node.reporting_mode = SH_REPORTING_HYBRID;
    copyText(node.sensor_mask, sizeof(node.sensor_mask), "XXXXXXXXXX");
    copyText(node.input_mask, sizeof(node.input_mask), "XXXXX");
    copyText(node.device_name, sizeof(node.device_name), "unknown");
}

void initialisiereNodeStates() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        initialisiereNodeSlot(nodeStates[i]);
    }
}

bool nodeHasCap(size_t nodeIndex, uint16_t cap) {
    return (nodeStates[nodeIndex].caps & cap) != 0U;
}

int findeNodeIndex(const char* nodeId) {
    if (!nodeId) return -1;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) continue;
        if (strncmp(nodeStates[i].device_id, nodeId, SH_DEVICE_ID_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int findeNodeIndexPerMac(const uint8_t* mac) {
    if (!mac) return -1;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].mac_bekannt) continue;
        if (memcmp(nodeStates[i].mac, mac, 6) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int findeFreienNodeIndex() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) return (int)i;
    }
    return -1;
}

uint16_t holeHelloCaps(const SmartHome::HelloPayload& payload) {
    return (uint16_t)(((uint16_t)payload.caps_hi << 8) | payload.caps_lo);
}

void setzeNetSenZusatzwerteUnbekannt(size_t nodeIndex) {
    nodeStates[nodeIndex].pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
    nodeStates[nodeIndex].aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
    nodeStates[nodeIndex].tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
    nodeStates[nodeIndex].eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
}

void sanitisiereNodeStateNachCapabilities(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].meta_bekannt) return;

    if (!nodeHasCap(nodeIndex, SH_CAP_RELAY)) nodeStates[nodeIndex].relay_1 = false;
    if (!nodeHasCap(nodeIndex, SH_CAP_RELAY2)) nodeStates[nodeIndex].relay_2 = false;
    if (!nodeHasCap(nodeIndex, SH_CAP_TEMP)) nodeStates[nodeIndex].temp_01c = INT16_MIN;
    if (!nodeHasCap(nodeIndex, SH_CAP_HUM)) nodeStates[nodeIndex].hum_01pct = 0xFFFFU;
    if (!nodeHasCap(nodeIndex, SH_CAP_LUX)) nodeStates[nodeIndex].lux = 0xFFFFU;
    if (!nodeHasCap(nodeIndex, SH_CAP_PRESSURE)) nodeStates[nodeIndex].pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_AQI)) {
        nodeStates[nodeIndex].aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
        nodeStates[nodeIndex].tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
        nodeStates[nodeIndex].eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
    }
    if (!nodeHasCap(nodeIndex, SH_CAP_MOTION)) nodeStates[nodeIndex].motion = false;
    if (!nodeHasCap(nodeIndex, SH_CAP_WINDOW)) nodeStates[nodeIndex].window_open = WINDOW_STATE_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_RAIN)) nodeStates[nodeIndex].rain_raw = RAIN_RAW_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_BATTERY)) {
        nodeStates[nodeIndex].battery_pct = BATTERY_PCT_UNGUELTIG;
        nodeStates[nodeIndex].battery_mv = BATTERY_MV_UNGUELTIG;
    }
    if (!nodeHasCap(nodeIndex, SH_CAP_COVER)) {
        nodeStates[nodeIndex].cover_mode = false;
        nodeStates[nodeIndex].cover_state = SH_COVER_STATE_STOPPED;
        nodeStates[nodeIndex].cover_position = 0U;
        nodeStates[nodeIndex].cover_calibrated = false;
    }
}

const char* availabilityStateText(size_t nodeIndex) {
    if (nodeStates[nodeIndex].online) {
        return "online";
    }
    const unsigned long letzterKontakt = nodeStates[nodeIndex].letzter_kontakt_ms;
    if (letzterKontakt == 0UL) return "offline";
    const unsigned long delta = millis() - letzterKontakt;
    const unsigned long timeout = offlineTimeoutMsForPowerType(nodeStates[nodeIndex].power_type);
    if (nodeStates[nodeIndex].power_type == SH_POWER_BATTERY) {
        if (delta <= timeout) return "asleep";
        if (delta <= (timeout * 2UL)) return "late";
        return "offline";
    }
    return delta <= timeout ? "late" : "offline";
}

bool stellePeerSicher(const uint8_t* mac) {
    if (!mac || !SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = (uint8_t)WLAN_KANAL;
    peerInfo.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        logf("WARN", "Peer konnte nicht angelegt werden (err=%d)", (int)err);
        return false;
    }

    char text[18] = {0};
    macText(mac, text, sizeof(text));
    logf("INFO", "Peer aktiv: %s", text);
    return true;
}

bool sendePaketMitOptionen(
    const uint8_t* zielMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label,
    uint8_t flags,
    bool festeSeq,
    uint8_t seq,
    uint8_t* verwendeteSeq)
{
    if (zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t effektiveSeq = festeSeq ? seq : masterStatus.naechste_seq++;
    SmartHome::fillHeader(header, msgType, effektiveSeq, flags, (uint16_t)payloadLen);

    uint8_t* payloadBuffer = buffer + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(buffer, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, buffer, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label, (int)err);
        return false;
    }

    char text[18] = {0};
    macText(zielMac, text, sizeof(text));
    logf("INFO", "%s gesendet an %s", label, text);
    if (verwendeteSeq != nullptr) {
        *verwendeteSeq = effektiveSeq;
    }
    return true;
}

bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    return sendePaketMitOptionen(zielMac, msgType, payload, payloadLen, label, 0U, false, 0U, nullptr);
}

void baueMasterTopic(const char* channel, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "smarthome/master/%s/%s", DEVICE_ID, channel);
}

void baueNodeTopicAusId(const char* deviceId, const char* suffix, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "smarthome/device/%s/%s", deviceId ? deviceId : "unknown", suffix);
}

void baueNodeTopic(size_t nodeIndex, const char* suffix, char* buffer, size_t bufferSize) {
    baueNodeTopicAusId(nodeStates[nodeIndex].device_id, suffix, buffer, bufferSize);
}

void publishRetained(const char* topic, const char* payload) {
    if (!masterStatus.mqtt_verbunden) return;
    if (!mqttClient.publish(topic, payload, true)) {
        logf("WARN", "MQTT retained publish fehlgeschlagen: %s", topic);
        return;
    }
    logf("INFO", "MQTT retained %s -> %s", topic, payload);
}

void publishTransient(const char* topic, const char* payload) {
    if (!masterStatus.mqtt_verbunden) return;
    if (!mqttClient.publish(topic, payload, false)) {
        logf("WARN", "MQTT publish fehlgeschlagen: %s", topic);
        return;
    }
    logf("INFO", "MQTT publish %s -> %s", topic, payload);
}

void baueMasterStatusJson(char* buffer, size_t bufferSize, bool online) {
    snprintf(
        buffer,
        bufferSize,
        "{\"master_id\":\"%s\",\"online\":%s,\"wifi\":%s,\"mqtt\":%s,\"espnow\":%s,\"fw\":\"%s\"}",
        DEVICE_ID,
        online ? "true" : "false",
        masterStatus.wlan_verbunden ? "true" : "false",
        masterStatus.mqtt_verbunden ? "true" : "false",
        masterStatus.espnow_bereit ? "true" : "false",
        PROJECT_VERSION);
}

void baueMasterEventJson(char* buffer, size_t bufferSize, const char* eventName) {
    snprintf(
        buffer,
        bufferSize,
        "{\"master_id\":\"%s\",\"event\":\"%s\",\"fw\":\"%s\"}",
        DEVICE_ID,
        eventName ? eventName : "unknown",
        PROJECT_VERSION);
}

void baueNodeMetaJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    char macBuffer[18] = {0};
    macText(nodeStates[nodeIndex].mac_bekannt ? nodeStates[nodeIndex].mac : nullptr, macBuffer, sizeof(macBuffer));

    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"device_name\":\"%s\",\"device_class\":\"%s\",\"power_type\":\"%s\",\"fw_version\":%u,\"caps\":%u,\"mac_address\":\"%s\",\"meta_schema_version\":%u,\"control_mode\":\"%s\",\"config_profile\":\"%s\",\"reporting_mode\":\"%s\",\"sensor_mask\":\"%s\",\"input_mask\":\"%s\"}",
        nodeStates[nodeIndex].device_id,
        nodeStates[nodeIndex].device_name,
        deviceClassText(nodeStates[nodeIndex].device_class),
        powerTypeText(nodeStates[nodeIndex].power_type),
        (unsigned)nodeStates[nodeIndex].fw_version,
        (unsigned)nodeStates[nodeIndex].caps,
        macBuffer,
        (unsigned)nodeStates[nodeIndex].meta_schema_version,
        controlModeText(nodeStates[nodeIndex].control_mode),
        configProfileText(nodeStates[nodeIndex].config_profile),
        reportingModeText(nodeStates[nodeIndex].reporting_mode),
        nodeStates[nodeIndex].sensor_mask,
        nodeStates[nodeIndex].input_mask);
}

void baueNodeAvailabilityJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    const char* availability = availabilityStateText(nodeIndex);
    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"availability\":\"%s\",\"online\":%s,\"power_type\":\"%s\"}",
        nodeStates[nodeIndex].device_id,
        availability,
        nodeStates[nodeIndex].online ? "true" : "false",
        powerTypeText(nodeStates[nodeIndex].power_type));
}

void schreibeIntOrNull(char* buffer, size_t bufferSize, long value, long invalidValue) {
    if (value == invalidValue) {
        copyText(buffer, bufferSize, "null");
        return;
    }
    snprintf(buffer, bufferSize, "%ld", value);
}

void schreibeUIntOrNull(char* buffer, size_t bufferSize, unsigned long value, unsigned long invalidValue) {
    if (value == invalidValue) {
        copyText(buffer, bufferSize, "null");
        return;
    }
    snprintf(buffer, bufferSize, "%lu", value);
}

void baueNodeStateJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    char tempText[16] = {0};
    char humText[16] = {0};
    char luxText[16] = {0};
    char pressureText[16] = {0};
    char gasText[16] = {0};
    char aqiText[16] = {0};
    char tvocText[16] = {0};
    char eco2Text[16] = {0};
    char batteryPctText[16] = {0};
    char batteryMvText[16] = {0};
    char windowText[16] = {0};
    char rainText[16] = {0};
    char coverPositionText[16] = {0};

    switch (nodeStates[nodeIndex].device_class) {
        case SH_CLASS_NET_ERL:
            schreibeIntOrNull(tempText, sizeof(tempText), nodeStates[nodeIndex].temp_01c, INT16_MIN);
            schreibeUIntOrNull(humText, sizeof(humText), nodeStates[nodeIndex].hum_01pct, 0xFFFFU);
            schreibeUIntOrNull(luxText, sizeof(luxText), nodeStates[nodeIndex].lux, 0xFFFFU);
            schreibeUIntOrNull(pressureText, sizeof(pressureText), nodeStates[nodeIndex].pressure_pa, NET_SEN_PRESSURE_UNGUELTIG);
            schreibeUIntOrNull(gasText, sizeof(gasText), nodeStates[nodeIndex].gas_ohm, NET_SEN_GAS_OHM_UNGUELTIG);
            schreibeUIntOrNull(aqiText, sizeof(aqiText), nodeStates[nodeIndex].aqi, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(tvocText, sizeof(tvocText), nodeStates[nodeIndex].tvoc_ppb, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(eco2Text, sizeof(eco2Text), nodeStates[nodeIndex].eco2_ppm, NET_SEN_AIR_METRIC_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"relay_1\":%s,\"temp_01c\":%s,\"hum_01pct\":%s,\"lux\":%s,\"pressure_pa\":%s,\"gas_ohm\":%s,\"aqi\":%s,\"tvoc_ppb\":%s,\"eco2_ppm\":%s,\"motion\":%s,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                nodeStates[nodeIndex].relay_1 ? "true" : "false",
                tempText,
                humText,
                luxText,
                pressureText,
                gasText,
                aqiText,
                tvocText,
                eco2Text,
                nodeStates[nodeIndex].motion ? "true" : "false",
                nodeStates[nodeIndex].fault ? "true" : "false");
            return;

        case SH_CLASS_NET_ZRL:
            if (nodeStates[nodeIndex].cover_calibrated) {
                schreibeUIntOrNull(coverPositionText, sizeof(coverPositionText), nodeStates[nodeIndex].cover_position, 255U);
                snprintf(
                    buffer,
                    bufferSize,
                    "{\"device_id\":\"%s\",\"relay_1\":%s,\"relay_2\":%s,\"cover_mode\":%s,\"cover_state\":\"%s\",\"cover_position\":%s,\"cover_calibrated\":true,\"fault\":%s}",
                    nodeStates[nodeIndex].device_id,
                    nodeStates[nodeIndex].relay_1 ? "true" : "false",
                    nodeStates[nodeIndex].relay_2 ? "true" : "false",
                    nodeStates[nodeIndex].cover_mode ? "true" : "false",
                    coverStateText(nodeStates[nodeIndex].cover_state, true, nodeStates[nodeIndex].cover_position),
                    coverPositionText,
                    nodeStates[nodeIndex].fault ? "true" : "false");
            } else {
                snprintf(
                    buffer,
                    bufferSize,
                    "{\"device_id\":\"%s\",\"relay_1\":%s,\"relay_2\":%s,\"cover_mode\":%s,\"cover_state\":\"%s\",\"cover_calibrated\":false,\"fault\":%s}",
                    nodeStates[nodeIndex].device_id,
                    nodeStates[nodeIndex].relay_1 ? "true" : "false",
                    nodeStates[nodeIndex].relay_2 ? "true" : "false",
                    nodeStates[nodeIndex].cover_mode ? "true" : "false",
                    coverStateText(nodeStates[nodeIndex].cover_state, false, 0U),
                    nodeStates[nodeIndex].fault ? "true" : "false");
            }
            return;

        case SH_CLASS_NET_SEN:
            schreibeIntOrNull(tempText, sizeof(tempText), nodeStates[nodeIndex].temp_01c, INT16_MIN);
            schreibeUIntOrNull(humText, sizeof(humText), nodeStates[nodeIndex].hum_01pct, 0xFFFFU);
            schreibeUIntOrNull(luxText, sizeof(luxText), nodeStates[nodeIndex].lux, 0xFFFFU);
            schreibeUIntOrNull(pressureText, sizeof(pressureText), nodeStates[nodeIndex].pressure_pa, NET_SEN_PRESSURE_UNGUELTIG);
            schreibeUIntOrNull(gasText, sizeof(gasText), nodeStates[nodeIndex].gas_ohm, NET_SEN_GAS_OHM_UNGUELTIG);
            schreibeUIntOrNull(aqiText, sizeof(aqiText), nodeStates[nodeIndex].aqi, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(tvocText, sizeof(tvocText), nodeStates[nodeIndex].tvoc_ppb, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(eco2Text, sizeof(eco2Text), nodeStates[nodeIndex].eco2_ppm, NET_SEN_AIR_METRIC_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"temp_01c\":%s,\"hum_01pct\":%s,\"lux\":%s,\"pressure_pa\":%s,\"gas_ohm\":%s,\"aqi\":%s,\"tvoc_ppb\":%s,\"eco2_ppm\":%s,\"motion\":%s,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                tempText,
                humText,
                luxText,
                pressureText,
                gasText,
                aqiText,
                tvocText,
                eco2Text,
                nodeStates[nodeIndex].motion ? "true" : "false",
                nodeStates[nodeIndex].fault ? "true" : "false");
            return;

        case SH_CLASS_BAT_SEN:
            schreibeUIntOrNull(batteryPctText, sizeof(batteryPctText), nodeStates[nodeIndex].battery_pct, BATTERY_PCT_UNGUELTIG);
            schreibeUIntOrNull(batteryMvText, sizeof(batteryMvText), nodeStates[nodeIndex].battery_mv, BATTERY_MV_UNGUELTIG);
            schreibeUIntOrNull(windowText, sizeof(windowText), nodeStates[nodeIndex].window_open, WINDOW_STATE_UNGUELTIG);
            schreibeUIntOrNull(rainText, sizeof(rainText), nodeStates[nodeIndex].rain_raw, RAIN_RAW_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"battery_pct\":%s,\"battery_mv\":%s,\"window_open\":%s,\"rain_raw\":%s,\"button_flags\":%u,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                batteryPctText,
                batteryMvText,
                windowText,
                rainText,
                (unsigned)nodeStates[nodeIndex].button_flags,
                nodeStates[nodeIndex].fault ? "true" : "false");
            return;

        default:
            copyText(buffer, bufferSize, "{}");
            return;
    }
}

const char* eventTypeText(uint8_t eventType) {
    switch (eventType) {
        case SH_EVENT_BUTTON_PRESS: return "button_press";
        case SH_EVENT_BUTTON_RELEASE: return "button_release";
        case SH_EVENT_BUTTON_LONG_PRESS: return "button_long_press";
        case SH_EVENT_MOTION_DETECTED: return "motion_detected";
        case SH_EVENT_WINDOW_OPENED: return "window_opened";
        case SH_EVENT_WINDOW_CLOSED: return "window_closed";
        case SH_EVENT_RAIN_DETECTED: return "rain_detected";
        case SH_EVENT_RELAY_CHANGED: return "relay_changed";
        case SH_EVENT_LIGHT_AUTO_ON: return "light_auto_on";
        case SH_EVENT_LIGHT_AUTO_OFF: return "light_auto_off";
        case SH_EVENT_COVER_UP: return "cover_up";
        case SH_EVENT_COVER_DOWN: return "cover_down";
        case SH_EVENT_COVER_STOP: return "cover_stop";
        case SH_EVENT_COVER_CALIB_START: return "cover_calib_start";
        case SH_EVENT_COVER_CALIB_DONE: return "cover_calib_done";
        case SH_EVENT_NODE_BOOT: return "node_boot";
        case SH_EVENT_SENSOR_FAULT: return "sensor_fault";
        case SH_EVENT_COMM_FAULT: return "comm_fault";
        default: return "unknown";
    }
}

void baueNodeEventJson(size_t nodeIndex, const SmartHome::EventReportPayload& payload, char* buffer, size_t bufferSize) {
    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"event\":\"%s\",\"event_type\":%u,\"trigger\":%u,\"param1\":%u,\"param2\":%u}",
        nodeStates[nodeIndex].device_id,
        eventTypeText(payload.event_type),
        (unsigned)payload.event_type,
        (unsigned)payload.trigger,
        (unsigned)payload.param1,
        (unsigned)payload.param2);
}

void publishMasterStatus() {
    char topic[96] = {0};
    char payload[192] = {0};
    baueMasterTopic("status", topic, sizeof(topic));
    baueMasterStatusJson(payload, sizeof(payload), true);
    publishRetained(topic, payload);
}

void publishMasterEvent(const char* eventName) {
    char topic[96] = {0};
    char payload[160] = {0};
    baueMasterTopic("event", topic, sizeof(topic));
    baueMasterEventJson(payload, sizeof(payload), eventName);
    publishTransient(topic, payload);
}

void publishNodeMeta(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].belegt || !nodeStates[nodeIndex].meta_bekannt) return;
    char topic[96] = {0};
    char payload[640] = {0};
    baueNodeTopic(nodeIndex, "meta", topic, sizeof(topic));
    baueNodeMetaJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

void publishNodeAvailability(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].belegt) return;
    char topic[96] = {0};
    char payload[192] = {0};
    baueNodeTopic(nodeIndex, "availability", topic, sizeof(topic));
    baueNodeAvailabilityJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

void publishNodeState(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].belegt || !nodeStates[nodeIndex].state_bekannt) return;
    char topic[96] = {0};
    char payload[512] = {0};
    baueNodeTopic(nodeIndex, "state", topic, sizeof(topic));
    baueNodeStateJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

void publishNodeEvent(size_t nodeIndex, const SmartHome::EventReportPayload& payload) {
    if (!nodeStates[nodeIndex].belegt) return;
    char topic[96] = {0};
    char json[224] = {0};
    baueNodeTopic(nodeIndex, "event", topic, sizeof(topic));
    baueNodeEventJson(nodeIndex, payload, json, sizeof(json));
    publishTransient(topic, json);
}

void publishNodeAckById(const char* deviceId, const char* requestId, const char* channel, const char* statusText, int statusCode, uint8_t ackMsgType, uint8_t ackSeq, const char* source) {
    char topic[96] = {0};
    char payload[384] = {0};
    baueNodeTopicAusId(deviceId, "ack", topic, sizeof(topic));
    snprintf(
        payload,
        sizeof(payload),
        "{\"device_id\":\"%s\",\"request_id\":\"%s\",\"channel\":\"%s\",\"status\":\"%s\",\"status_code\":%d,\"ack_msg_type\":%u,\"ack_seq\":%u,\"source\":\"%s\"}",
        deviceId ? deviceId : "unknown",
        requestId ? requestId : "",
        channel ? channel : "command",
        statusText ? statusText : "unknown",
        statusCode,
        (unsigned)ackMsgType,
        (unsigned)ackSeq,
        source ? source : "master");
    publishTransient(topic, payload);
}

void publishNodeAck(size_t nodeIndex, const char* requestId, const char* channel, const char* statusText, int statusCode, uint8_t ackMsgType, uint8_t ackSeq, const char* source) {
    publishNodeAckById(nodeStates[nodeIndex].device_id, requestId, channel, statusText, statusCode, ackMsgType, ackSeq, source);
}

void publishBekannteNodesNachReconnect() {
    publishMasterStatus();
    publishMasterEvent("mqtt_connected");
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) continue;
        publishNodeMeta(i);
        publishNodeAvailability(i);
        publishNodeState(i);
    }
}

void aktualisiereNodeKontakt(size_t nodeIndex, const uint8_t* mac) {
    nodeStates[nodeIndex].letzter_kontakt_ms = millis();

    if (mac != nullptr) {
        const bool neueMac = !nodeStates[nodeIndex].mac_bekannt || memcmp(nodeStates[nodeIndex].mac, mac, 6) != 0;
        memcpy(nodeStates[nodeIndex].mac, mac, 6);
        nodeStates[nodeIndex].mac_bekannt = true;
        stellePeerSicher(nodeStates[nodeIndex].mac);
        if (neueMac) {
            char text[18] = {0};
            macText(mac, text, sizeof(text));
            logf("INFO", "%s MAC aktualisiert: %s", nodeStates[nodeIndex].device_id, text);
        }
    }

    const bool warOffline = !nodeStates[nodeIndex].online;
    nodeStates[nodeIndex].online = true;
    if (warOffline) {
        publishNodeAvailability(nodeIndex);
        logf("INFO", "Node %s ist online", nodeStates[nodeIndex].device_id);
    }
}

void sendeHelloAck(const uint8_t* zielMac, uint8_t ackStatus) {
    SmartHome::HelloAckPayload payload = {};
    payload.channel = (uint8_t)(masterStatus.wlan_verbunden ? WiFi.channel() : WLAN_KANAL);
    payload.ack_status = ackStatus;
    sendePaket(zielMac, SH_MSG_HELLO_ACK, &payload, sizeof(payload), "HELLO_ACK");
}

int registriereOderFindeNode(const uint8_t* senderMac, const SmartHome::HelloPayload& payload) {
    if (!SmartHome::isValidDeviceId(payload.device_id)) {
        logf("WARN", "HELLO abgelehnt: ungueltige device_id=%s", payload.device_id);
        return -1;
    }
    if (!istDeviceClassGueltig(payload.device_class) || !istPowerTypeGueltig(payload.power_type)) {
        logf("WARN", "HELLO abgelehnt: class=%u power=%u", (unsigned)payload.device_class, (unsigned)payload.power_type);
        return -1;
    }
    const int idIndex = findeNodeIndex(payload.device_id);
    const int macIndex = findeNodeIndexPerMac(senderMac);

    if (idIndex >= 0) {
        if (nodeStates[idIndex].device_class != payload.device_class || nodeStates[idIndex].power_type != payload.power_type) {
            logf("WARN", "HELLO abgelehnt: bestehende node %s hat andere class/power", payload.device_id);
            return -1;
        }
        if (macIndex >= 0 && macIndex != idIndex) {
            logf("WARN", "HELLO abgelehnt: MAC-Konflikt fuer %s", payload.device_id);
            return -1;
        }
        return idIndex;
    }

    if (macIndex >= 0 && strncmp(nodeStates[macIndex].device_id, payload.device_id, SH_DEVICE_ID_LEN) != 0) {
        logf("WARN", "HELLO abgelehnt: bekannte MAC gehoert bereits zu %s", nodeStates[macIndex].device_id);
        return -1;
    }

    const int freeIndex = findeFreienNodeIndex();
    if (freeIndex < 0) {
        logf("WARN", "HELLO abgelehnt: Registry voll (device_id=%s)", payload.device_id);
        return -1;
    }

    initialisiereNodeSlot(nodeStates[freeIndex]);
    nodeStates[freeIndex].belegt = true;
    nodeStates[freeIndex].device_class = payload.device_class;
    nodeStates[freeIndex].power_type = payload.power_type;
    copyText(nodeStates[freeIndex].device_id, sizeof(nodeStates[freeIndex].device_id), payload.device_id);
    logf("INFO", "Node dynamisch registriert: %s (%s)", payload.device_id, deviceClassText(payload.device_class));
    return freeIndex;
}

void verarbeiteHello(const uint8_t* senderMac, const SmartHome::HelloPayload& payload) {
    const int nodeIndex = registriereOderFindeNode(senderMac, payload);
    if (nodeIndex < 0) {
        sendeHelloAck(senderMac, SH_ACK_REJECTED);
        return;
    }

    nodeStates[nodeIndex].meta_bekannt = true;
    nodeStates[nodeIndex].caps = holeHelloCaps(payload);
    nodeStates[nodeIndex].fw_version = payload.fw_version;
    nodeStates[nodeIndex].meta_schema_version = payload.meta_schema_version;
    nodeStates[nodeIndex].control_mode = payload.control_mode;
    nodeStates[nodeIndex].config_profile = payload.config_profile;
    nodeStates[nodeIndex].reporting_mode = payload.reporting_mode;
    copyText(nodeStates[nodeIndex].device_name, sizeof(nodeStates[nodeIndex].device_name), payload.device_name);
    copyText(nodeStates[nodeIndex].sensor_mask, sizeof(nodeStates[nodeIndex].sensor_mask), payload.sensor_mask);
    copyText(nodeStates[nodeIndex].input_mask, sizeof(nodeStates[nodeIndex].input_mask), payload.input_mask);
    sanitisiereNodeStateNachCapabilities((size_t)nodeIndex);
    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);

    publishNodeMeta((size_t)nodeIndex);
    publishNodeAvailability((size_t)nodeIndex);
    if (nodeStates[nodeIndex].state_bekannt) {
        publishNodeState((size_t)nodeIndex);
    }

    sendeHelloAck(senderMac, SH_ACK_OK);
    logf("INFO", "HELLO von %s (%s)", payload.device_id, payload.device_name);
}

void verarbeiteHeartbeat(const uint8_t* senderMac, const SmartHome::HeartbeatPayload& payload) {
    const int nodeIndex = findeNodeIndex(payload.node_id);
    if (nodeIndex < 0) {
        logf("WARN", "HEARTBEAT ignoriert: unbekannte node_id=%s", payload.node_id);
        return;
    }
    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);
    nodeStates[nodeIndex].uptime_s = payload.uptime_s;
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "HEARTBEAT von %s (uptime=%lus)", payload.node_id, (unsigned long)payload.uptime_s);
}

void verarbeiteStateReport(const uint8_t* senderMac, const uint8_t* payload, uint16_t payloadLen) {
    if (!payload || payloadLen < SH_DEVICE_ID_LEN) {
        logf("WARN", "STATE_REPORT verworfen: payload ungueltig");
        return;
    }

    const char* nodeId = reinterpret_cast<const char*>(payload);
    const int nodeIndex = findeNodeIndex(nodeId);
    if (nodeIndex < 0) {
        logf("WARN", "STATE_REPORT ignoriert: unbekannte node_id=%s", nodeId);
        return;
    }

    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);

    switch (nodeStates[nodeIndex].device_class) {
        case SH_CLASS_NET_ERL: {
            if (payloadLen == sizeof(SmartHome::StateReportPayload)) {
                const SmartHome::StateReportPayload& state = *reinterpret_cast<const SmartHome::StateReportPayload*>(payload);
                nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            if (payloadLen == sizeof(SmartHome::RelayComfortStateReportPayload) ||
                payloadLen == sizeof(SmartHome::RelayComfortConfigStateReportPayload)) {
                const SmartHome::RelayComfortStateReportPayload& state = *reinterpret_cast<const SmartHome::RelayComfortStateReportPayload*>(payload);
                nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
                nodeStates[nodeIndex].temp_01c = state.temp_01c;
                nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
                nodeStates[nodeIndex].lux = state.lux;
                nodeStates[nodeIndex].motion = (state.motion != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeId);
            return;
        }

        case SH_CLASS_NET_ZRL: {
            if (payloadLen == sizeof(SmartHome::ZrlStateReportPayload)) {
                const SmartHome::ZrlStateReportPayload& state = *reinterpret_cast<const SmartHome::ZrlStateReportPayload*>(payload);
                nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
                nodeStates[nodeIndex].relay_2 = (state.relay_2 != 0U);
                nodeStates[nodeIndex].cover_mode = (state.cover_mode != 0U);
                nodeStates[nodeIndex].cover_state = state.cover_state;
                nodeStates[nodeIndex].cover_position = state.cover_position;
                nodeStates[nodeIndex].cover_calibrated = (state.cover_calibrated != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            if (payloadLen == sizeof(SmartHome::ZrlConfigStateReportPayload)) {
                const SmartHome::ZrlConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::ZrlConfigStateReportPayload*>(payload);
                nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
                nodeStates[nodeIndex].relay_2 = (state.relay_2 != 0U);
                nodeStates[nodeIndex].cover_mode = (state.cover_mode != 0U);
                nodeStates[nodeIndex].cover_state = state.cover_state;
                nodeStates[nodeIndex].cover_position = state.cover_position;
                nodeStates[nodeIndex].cover_calibrated = (state.cover_calibrated != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeId);
            return;
        }

        case SH_CLASS_NET_SEN: {
            if (payloadLen == sizeof(SmartHome::SensorStateReportPayload)) {
                const SmartHome::SensorStateReportPayload& state = *reinterpret_cast<const SmartHome::SensorStateReportPayload*>(payload);
                nodeStates[nodeIndex].temp_01c = state.temp_01c;
                nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
                nodeStates[nodeIndex].lux = state.lux;
                nodeStates[nodeIndex].motion = (state.motion != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                setzeNetSenZusatzwerteUnbekannt((size_t)nodeIndex);
                break;
            }
            if (payloadLen == sizeof(SmartHome::SensorConfigStateReportPayload)) {
                const SmartHome::SensorConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::SensorConfigStateReportPayload*>(payload);
                nodeStates[nodeIndex].temp_01c = state.temp_01c;
                nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
                nodeStates[nodeIndex].lux = state.lux;
                nodeStates[nodeIndex].motion = (state.motion != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                setzeNetSenZusatzwerteUnbekannt((size_t)nodeIndex);
                break;
            }
            if (payloadLen == sizeof(SmartHome::ExtendedSensorStateReportPayload) ||
                payloadLen == sizeof(SmartHome::ExtendedSensorConfigStateReportPayload)) {
                const SmartHome::ExtendedSensorStateReportPayload& state = *reinterpret_cast<const SmartHome::ExtendedSensorStateReportPayload*>(payload);
                nodeStates[nodeIndex].temp_01c = state.temp_01c;
                nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
                nodeStates[nodeIndex].lux = state.lux;
                nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
                nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
                nodeStates[nodeIndex].aqi = state.aqi;
                nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
                nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
                nodeStates[nodeIndex].motion = (state.motion != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            if (payloadLen == sizeof(SmartHome::ExtendedSensorGasStateReportPayload) ||
                payloadLen == sizeof(SmartHome::ExtendedSensorGasConfigStateReportPayload)) {
                const SmartHome::ExtendedSensorGasStateReportPayload& state = *reinterpret_cast<const SmartHome::ExtendedSensorGasStateReportPayload*>(payload);
                nodeStates[nodeIndex].temp_01c = state.temp_01c;
                nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
                nodeStates[nodeIndex].lux = state.lux;
                nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
                nodeStates[nodeIndex].gas_ohm = state.gas_ohm;
                nodeStates[nodeIndex].aqi = state.aqi;
                nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
                nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
                nodeStates[nodeIndex].motion = (state.motion != 0U);
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeId);
            return;
        }

        case SH_CLASS_BAT_SEN: {
            if (payloadLen == sizeof(SmartHome::BatteryStateReportPayload)) {
                const SmartHome::BatteryStateReportPayload& state = *reinterpret_cast<const SmartHome::BatteryStateReportPayload*>(payload);
                nodeStates[nodeIndex].battery_pct = state.battery_pct;
                nodeStates[nodeIndex].battery_mv = state.battery_mv;
                nodeStates[nodeIndex].window_open = state.window_open;
                nodeStates[nodeIndex].rain_raw = state.rain_raw;
                nodeStates[nodeIndex].button_flags = state.button_flags;
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            if (payloadLen == sizeof(SmartHome::BatteryConfigStateReportPayload)) {
                const SmartHome::BatteryConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::BatteryConfigStateReportPayload*>(payload);
                nodeStates[nodeIndex].battery_pct = state.battery_pct;
                nodeStates[nodeIndex].battery_mv = state.battery_mv;
                nodeStates[nodeIndex].window_open = state.window_open;
                nodeStates[nodeIndex].rain_raw = state.rain_raw;
                nodeStates[nodeIndex].button_flags = state.button_flags;
                nodeStates[nodeIndex].fault = (state.fault != 0U);
                break;
            }
            logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeId);
            return;
        }

        default:
            logf("WARN", "STATE_REPORT ohne Handler fuer %s", nodeId);
            return;
    }

    nodeStates[nodeIndex].state_bekannt = true;
    sanitisiereNodeStateNachCapabilities((size_t)nodeIndex);
    publishNodeState((size_t)nodeIndex);
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "STATE_REPORT von %s verarbeitet", nodeId);
}

void verarbeiteEventReport(const uint8_t* senderMac, const SmartHome::EventReportPayload& payload) {
    const int nodeIndex = findeNodeIndex(payload.node_id);
    if (nodeIndex < 0) {
        logf("WARN", "EVENT ignoriert: unbekannte node_id=%s", payload.node_id);
        return;
    }

    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);
    publishNodeEvent((size_t)nodeIndex, payload);
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "EVENT von %s: type=%u", payload.node_id, (unsigned)payload.event_type);
}

void verarbeiteAck(const uint8_t* senderMac, const SmartHome::AckPayload& payload) {
    const int nodeIndex = findeNodeIndexPerMac(senderMac);
    if (nodeIndex < 0) {
        logf("WARN", "ACK ignoriert: unbekannte Sender-MAC");
        return;
    }

    if (nodeStates[nodeIndex].pending_cfg.aktiv &&
        payload.ack_msg_type == SH_MSG_CFG &&
        payload.ack_seq == nodeStates[nodeIndex].pending_cfg.seq) {
        const char* statusText = payload.status == SH_ACK_OK ? "ok" : (payload.status == SH_ACK_REJECTED ? "rejected" : "error");
        publishNodeAck((size_t)nodeIndex, nodeStates[nodeIndex].pending_cfg.request_id, nodeStates[nodeIndex].pending_cfg.command_channel, statusText, (int)payload.status, payload.ack_msg_type, payload.ack_seq, "node_ack");
        nodeStates[nodeIndex].pending_cfg = {};
        return;
    }

    if (nodeStates[nodeIndex].pending_cmd.aktiv &&
        payload.ack_msg_type == SH_MSG_CMD &&
        payload.ack_seq == nodeStates[nodeIndex].pending_cmd.seq) {
        const char* statusText = payload.status == SH_ACK_OK ? "ok" : (payload.status == SH_ACK_REJECTED ? "rejected" : "error");
        publishNodeAck((size_t)nodeIndex, nodeStates[nodeIndex].pending_cmd.request_id, nodeStates[nodeIndex].pending_cmd.command_channel, statusText, (int)payload.status, payload.ack_msg_type, payload.ack_seq, "node_ack");
        nodeStates[nodeIndex].pending_cmd = {};
        return;
    }

    logf("WARN", "ACK ignoriert: passt zu keinem offenen Request");
}

void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* daten, int laenge) {
    if (!senderMac || !daten || laenge < (int)sizeof(SmartHome::MsgHeader)) {
        logf("WARN", "ESP-NOW Paket verworfen: ungueltige Eingabe");
        return;
    }

    if (!SmartHome::hasValidPacketCrc(daten, (size_t)laenge)) {
        logf("WARN", "ESP-NOW Paket verworfen: CRC/Header ungueltig");
        return;
    }

    const SmartHome::MsgHeader* header = reinterpret_cast<const SmartHome::MsgHeader*>(daten);
    const uint8_t* payload = daten + SH_HEADER_SIZE;

    switch (header->msg_type) {
        case SH_MSG_HELLO:
            if (header->payload_len == sizeof(SmartHome::HelloPayload)) {
                verarbeiteHello(senderMac, *reinterpret_cast<const SmartHome::HelloPayload*>(payload));
            }
            break;
        case SH_MSG_HEARTBEAT:
            if (header->payload_len == sizeof(SmartHome::HeartbeatPayload)) {
                verarbeiteHeartbeat(senderMac, *reinterpret_cast<const SmartHome::HeartbeatPayload*>(payload));
            }
            break;
        case SH_MSG_STATE:
            verarbeiteStateReport(senderMac, payload, header->payload_len);
            break;
        case SH_MSG_EVENT:
            if (header->payload_len == sizeof(SmartHome::EventReportPayload)) {
                verarbeiteEventReport(senderMac, *reinterpret_cast<const SmartHome::EventReportPayload*>(payload));
            }
            break;
        case SH_MSG_ACK:
            if (header->payload_len == sizeof(SmartHome::AckPayload)) {
                verarbeiteAck(senderMac, *reinterpret_cast<const SmartHome::AckPayload*>(payload));
            }
            break;
        default:
            logf("WARN", "ESP-NOW Nachricht ignoriert (msg_type=%u)", header->msg_type);
            break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* daten, int laenge) {
    if (info == nullptr) return;
    verarbeiteEspNowPaket(info->src_addr, daten, laenge);
}
#else
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* daten, int laenge) {
    verarbeiteEspNowPaket(senderMac, daten, laenge);
}
#endif

void onEspNowSent(const uint8_t* mac, esp_now_send_status_t status) {
    char text[18] = {0};
    macText(mac, text, sizeof(text));
    logf(
        status == ESP_NOW_SEND_SUCCESS ? "INFO" : "WARN",
        "ESP-NOW Sendestatus an %s: %s",
        text,
        status == ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}

bool sendeStateRequest(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "STATE_REQUEST verworfen: MAC fuer %s unbekannt", nodeStates[nodeIndex].device_id);
        return false;
    }

    SmartHome::CmdPayload payload = {};
    payload.cmd_type = SH_CMD_STATE_REQUEST;
    return sendePaket(nodeStates[nodeIndex].mac, SH_MSG_CMD, &payload, sizeof(payload), "STATE_REQUEST");
}

bool sendeCmdRequest(size_t nodeIndex, uint8_t cmdType, uint8_t param1, uint8_t param2, const char* requestId, const char* channel, const char* label) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "%s verworfen: MAC fuer %s unbekannt", label, nodeStates[nodeIndex].device_id);
        return false;
    }

    SmartHome::CmdPayload payload = {};
    payload.cmd_type = cmdType;
    payload.param1 = param1;
    payload.param2 = param2;

    uint8_t seq = 0U;
    const bool erfolgreich = sendePaketMitOptionen(
        nodeStates[nodeIndex].mac,
        SH_MSG_CMD,
        &payload,
        sizeof(payload),
        label,
        SH_FLAG_ACK_REQUEST,
        false,
        0U,
        &seq);

    if (!erfolgreich) return false;

    nodeStates[nodeIndex].pending_cmd = {};
    nodeStates[nodeIndex].pending_cmd.aktiv = true;
    nodeStates[nodeIndex].pending_cmd.seq = seq;
    nodeStates[nodeIndex].pending_cmd.retries = 0U;
    nodeStates[nodeIndex].pending_cmd.cmd_type = cmdType;
    nodeStates[nodeIndex].pending_cmd.param1 = param1;
    nodeStates[nodeIndex].pending_cmd.param2 = param2;
    nodeStates[nodeIndex].pending_cmd.letztes_senden_ms = millis();
    copyText(nodeStates[nodeIndex].pending_cmd.request_id, sizeof(nodeStates[nodeIndex].pending_cmd.request_id), requestId);
    copyText(nodeStates[nodeIndex].pending_cmd.command_channel, sizeof(nodeStates[nodeIndex].pending_cmd.command_channel), channel ? channel : "command");
    return true;
}

bool sendeRelayCommand(size_t nodeIndex, uint8_t relayIndex, bool relayState, const char* requestId, const char* channel) {
    return sendeCmdRequest(nodeIndex, SH_CMD_SET_RELAY, relayIndex, relayState ? 1U : 0U, requestId, channel, "COMMAND_SET_RELAY");
}

bool sendeCoverCommand(size_t nodeIndex, uint8_t coverAction, uint8_t position, const char* requestId, const char* channel) {
    return sendeCmdRequest(nodeIndex, SH_CMD_COVER, coverAction, position, requestId, channel, "COMMAND_COVER");
}

bool sendeConfigCommand(size_t nodeIndex, uint8_t paramId, uint16_t value, const char* requestId, const char* channel) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "CONFIG_SET verworfen: MAC fuer %s unbekannt", nodeStates[nodeIndex].device_id);
        return false;
    }

    SmartHome::CfgPayload payload = {};
    payload.param_id = paramId;
    payload.value = value;

    uint8_t seq = 0U;
    const bool erfolgreich = sendePaketMitOptionen(
        nodeStates[nodeIndex].mac,
        SH_MSG_CFG,
        &payload,
        sizeof(payload),
        "CONFIG_SET",
        SH_FLAG_ACK_REQUEST,
        false,
        0U,
        &seq);

    if (!erfolgreich) return false;

    nodeStates[nodeIndex].pending_cfg = {};
    nodeStates[nodeIndex].pending_cfg.aktiv = true;
    nodeStates[nodeIndex].pending_cfg.seq = seq;
    nodeStates[nodeIndex].pending_cfg.retries = 0U;
    nodeStates[nodeIndex].pending_cfg.param_id = paramId;
    nodeStates[nodeIndex].pending_cfg.value = value;
    nodeStates[nodeIndex].pending_cfg.letztes_senden_ms = millis();
    copyText(nodeStates[nodeIndex].pending_cfg.request_id, sizeof(nodeStates[nodeIndex].pending_cfg.request_id), requestId);
    copyText(nodeStates[nodeIndex].pending_cfg.command_channel, sizeof(nodeStates[nodeIndex].pending_cfg.command_channel), channel ? channel : "command");
    return true;
}

void loggeMqttConnectFehler() {
    if (mqttBrokerNutzeDirekteIp) {
        char brokerIpText[16] = "0.0.0.0";
        mqttBrokerIp.toString().toCharArray(brokerIpText, sizeof(brokerIpText));
        logf("WARN", "MQTT connect fehlgeschlagen (state=%d, broker=%s:%d, typ=%s)", mqttClient.state(), brokerIpText, MQTT_PORT, mqttBrokerTypText());
        return;
    }
    logf("WARN", "MQTT connect fehlgeschlagen (state=%d, broker=%s:%d, typ=%s)", mqttClient.state(), MQTT_HOST, MQTT_PORT, mqttBrokerTypText());
}

bool skipWhitespace(const char*& cursor) {
    if (cursor == nullptr) return false;
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) cursor++;
    return *cursor != '\0';
}

bool jsonHoleString(const char* json, const char* key, char* ziel, size_t zielGroesse) {
    if (!json || !key || !ziel || zielGroesse == 0U) return false;
    char muster[48] = {0};
    snprintf(muster, sizeof(muster), "\"%s\"", key);
    const char* fund = strstr(json, muster);
    if (!fund) return false;
    const char* cursor = strchr(fund + strlen(muster), ':');
    if (!cursor) return false;
    cursor++;
    if (!skipWhitespace(cursor) || *cursor != '"') return false;
    cursor++;
    const char* ende = strchr(cursor, '"');
    if (!ende) return false;
    size_t len = (size_t)(ende - cursor);
    if (len >= zielGroesse) len = zielGroesse - 1U;
    memcpy(ziel, cursor, len);
    ziel[len] = '\0';
    return true;
}

bool jsonHoleBool(const char* json, const char* key, bool* wert) {
    if (!json || !key || !wert) return false;
    char muster[48] = {0};
    snprintf(muster, sizeof(muster), "\"%s\"", key);
    const char* fund = strstr(json, muster);
    if (!fund) return false;
    const char* cursor = strchr(fund + strlen(muster), ':');
    if (!cursor) return false;
    cursor++;
    if (!skipWhitespace(cursor)) return false;
    if (strncmp(cursor, "true", 4) == 0) {
        *wert = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0) {
        *wert = false;
        return true;
    }
    return false;
}

bool jsonHoleZahl(const char* json, const char* key, long* wert) {
    if (!json || !key || !wert) return false;
    char muster[48] = {0};
    snprintf(muster, sizeof(muster), "\"%s\"", key);
    const char* fund = strstr(json, muster);
    if (!fund) return false;
    const char* cursor = strchr(fund + strlen(muster), ':');
    if (!cursor) return false;
    cursor++;
    if (!skipWhitespace(cursor)) return false;
    char* ende = nullptr;
    const long parsed = strtol(cursor, &ende, 10);
    if (ende == cursor) return false;
    if (*ende == '.' || *ende == 'e' || *ende == 'E') return false;
    *wert = parsed;
    return true;
}

bool jsonHoleObjekt(const char* json, const char* key, char* ziel, size_t zielGroesse) {
    if (!json || !key || !ziel || zielGroesse == 0U) return false;
    char muster[48] = {0};
    snprintf(muster, sizeof(muster), "\"%s\"", key);
    const char* fund = strstr(json, muster);
    if (!fund) return false;
    const char* cursor = strchr(fund + strlen(muster), ':');
    if (!cursor) return false;
    cursor++;
    if (!skipWhitespace(cursor) || *cursor != '{') return false;
    const char* start = cursor;
    int tiefe = 0;
    bool inString = false;
    bool escaped = false;
    while (*cursor != '\0') {
        const char zeichen = *cursor;
        if (inString) {
            if (escaped) escaped = false;
            else if (zeichen == '\\') escaped = true;
            else if (zeichen == '"') inString = false;
        } else {
            if (zeichen == '"') inString = true;
            else if (zeichen == '{') tiefe++;
            else if (zeichen == '}') {
                tiefe--;
                if (tiefe == 0) {
                    cursor++;
                    const size_t len = (size_t)(cursor - start);
                    if (len >= zielGroesse) return false;
                    memcpy(ziel, start, len);
                    ziel[len] = '\0';
                    return true;
                }
            }
        }
        cursor++;
    }
    return false;
}

bool parseSetConfigMinimal(size_t nodeIndex, const char* json, uint8_t* paramId, uint16_t* value, char* errorText, size_t errorSize) {
    if (!paramId || !value || !errorText || errorSize == 0U) return false;
    errorText[0] = '\0';

    char valuesJson[192] = {0};
    if (!jsonHoleObjekt(json, "values", valuesJson, sizeof(valuesJson))) {
        copyText(errorText, errorSize, "set_config verlangt ein values-Objekt");
        return false;
    }

    long numberValue = 0L;
    if (jsonHoleZahl(valuesJson, "report_interval_s", &numberValue)) {
        if (numberValue < CFG_REPORT_INTERVAL_MIN || numberValue > CFG_REPORT_INTERVAL_MAX) {
            snprintf(errorText, errorSize, "report_interval_s ausserhalb %ld..%ld", CFG_REPORT_INTERVAL_MIN, CFG_REPORT_INTERVAL_MAX);
            return false;
        }
        *paramId = SH_CFG_REPORT_INTERVAL_S;
        *value = (uint16_t)numberValue;
        return true;
    }

    snprintf(errorText, errorSize, "Kein unterstuetztes CFG-Feld fuer %s gefunden", deviceClassText(nodeStates[nodeIndex].device_class));
    return false;
}

bool istCoverGeraet(size_t nodeIndex) {
    return nodeStates[nodeIndex].control_mode == SH_CONTROL_MODE_COVER || nodeHasCap(nodeIndex, SH_CAP_COVER);
}

bool istRelayBefehlZulaessig(size_t nodeIndex, uint8_t relayIndex) {
    if (istCoverGeraet(nodeIndex)) return false;
    if (relayIndex == 0U) {
        return nodeHasCap(nodeIndex, SH_CAP_RELAY);
    }
    if (relayIndex == 1U) {
        return nodeHasCap(nodeIndex, SH_CAP_RELAY2);
    }
    return false;
}

uint8_t ackMsgTypeFuerCommand(const char* cmd) {
    return (cmd != nullptr && strcmp(cmd, "set_config") == 0) ? SH_MSG_CFG : SH_MSG_CMD;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char json[256] = {0};
    size_t copyLen = length;
    if (copyLen >= sizeof(json)) copyLen = sizeof(json) - 1U;
    memcpy(json, payload, copyLen);
    json[copyLen] = '\0';
    if (copyLen > 0U && json[copyLen - 1U] == '\n') {
        json[copyLen - 1U] = '\0';
    }

    const char* prefix = "smarthome/device/";
    if (strncmp(topic, prefix, strlen(prefix)) != 0) {
        logf("WARN", "MQTT Topic ignoriert: %s", topic);
        return;
    }

    char nodeId[SH_DEVICE_ID_LEN] = {0};
    const char* start = topic + strlen(prefix);
    const char* slash = strchr(start, '/');
    if (!slash || strcmp(slash, "/command") != 0) {
        logf("WARN", "MQTT Topic ignoriert: %s", topic);
        return;
    }

    size_t nodeLen = (size_t)(slash - start);
    if (nodeLen == 0U || nodeLen >= sizeof(nodeId)) {
        logf("WARN", "MQTT Topic ohne gueltige node_id: %s", topic);
        return;
    }
    memcpy(nodeId, start, nodeLen);
    nodeId[nodeLen] = '\0';

    char cmd[32] = {0};
    if (!jsonHoleString(json, "command", cmd, sizeof(cmd)) || cmd[0] == '\0') {
        logf("WARN", "MQTT command ohne gueltiges command-Feld fuer %s", nodeId);
        return;
    }

    logf("INFO", "MQTT empfangen %s -> %s", topic, json);

    const int nodeIndex = findeNodeIndex(nodeId);
    if (strcmp(cmd, "get_state") == 0) {
        if (nodeIndex < 0) {
            logf("WARN", "MQTT get_state fuer unbekannte device_id=%s", nodeId);
            return;
        }
        sendeStateRequest((size_t)nodeIndex);
        return;
    }

    char requestId[REQUEST_ID_LEN] = {0};
    if (!jsonHoleString(json, "request_id", requestId, sizeof(requestId)) || requestId[0] == '\0') {
        logf("WARN", "MQTT %s ohne request_id fuer %s verworfen", cmd, nodeId);
        return;
    }

    const char* commandChannel = "command";
    const uint8_t ackMsgType = ackMsgTypeFuerCommand(cmd);

    if (nodeIndex < 0) {
        publishNodeAckById(nodeId, requestId, commandChannel, "unknown_device", STATUS_CODE_UNKNOWN_DEVICE, ackMsgType, 0U, "master_registry");
        return;
    }

    if (strcmp(cmd, "set_relay") == 0) {
        if (nodeStates[nodeIndex].pending_cmd.aktiv) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CMD, nodeStates[nodeIndex].pending_cmd.seq, "master_busy");
            return;
        }

        bool relayState = false;
        if (jsonHoleBool(json, "relay_1", &relayState)) {
            if (!istRelayBefehlZulaessig((size_t)nodeIndex, 0U)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
                return;
            }
            if (!sendeRelayCommand((size_t)nodeIndex, 0U, relayState, requestId, commandChannel)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
            }
            return;
        }
        if (jsonHoleBool(json, "relay_2", &relayState)) {
            if (!istRelayBefehlZulaessig((size_t)nodeIndex, 1U)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
                return;
            }
            if (!sendeRelayCommand((size_t)nodeIndex, 1U, relayState, requestId, commandChannel)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
            }
            return;
        }

        publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "invalid_payload", -3, SH_MSG_CMD, 0U, "master_validation");
        return;
    }

    if (strcmp(cmd, "open") == 0 || strcmp(cmd, "close") == 0 || strcmp(cmd, "stop") == 0 || strcmp(cmd, "set_position") == 0) {
        if (!istCoverGeraet((size_t)nodeIndex)) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (nodeStates[nodeIndex].pending_cmd.aktiv) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CMD, nodeStates[nodeIndex].pending_cmd.seq, "master_busy");
            return;
        }

        if (strcmp(cmd, "open") == 0) {
            if (!sendeCoverCommand((size_t)nodeIndex, SH_COVER_CMD_OPEN, 0U, requestId, commandChannel)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
            }
            return;
        }
        if (strcmp(cmd, "close") == 0) {
            if (!sendeCoverCommand((size_t)nodeIndex, SH_COVER_CMD_CLOSE, 0U, requestId, commandChannel)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
            }
            return;
        }
        if (strcmp(cmd, "stop") == 0) {
            if (!sendeCoverCommand((size_t)nodeIndex, SH_COVER_CMD_STOP, 0U, requestId, commandChannel)) {
                publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
            }
            return;
        }

        long position = -1L;
        if (!jsonHoleZahl(json, "position", &position) || position < 0L || position > 100L) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "invalid_payload", -3, SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (!nodeStates[nodeIndex].cover_calibrated) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "not_calibrated", STATUS_CODE_NOT_CALIBRATED, SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (!sendeCoverCommand((size_t)nodeIndex, SH_COVER_CMD_SET_POSITION, (uint8_t)position, requestId, commandChannel)) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        }
        return;
    }

    if (strcmp(cmd, "set_config") == 0) {
        if (nodeStates[nodeIndex].pending_cfg.aktiv) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CFG, nodeStates[nodeIndex].pending_cfg.seq, "master_busy");
            return;
        }

        uint8_t paramId = 0U;
        uint16_t value = 0U;
        char errorText[96] = {0};
        if (!parseSetConfigMinimal((size_t)nodeIndex, json, &paramId, &value, errorText, sizeof(errorText))) {
            logf("WARN", "set_config fuer %s verworfen: %s", nodeId, errorText);
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "invalid_payload", -3, SH_MSG_CFG, 0U, "master_validation");
            return;
        }

        if (!sendeConfigCommand((size_t)nodeIndex, paramId, value, requestId, commandChannel)) {
            publishNodeAck((size_t)nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CFG, 0U, "master_send");
        }
        return;
    }

    logf("WARN", "MQTT Kommando ignoriert fuer %s", nodeId);
}

void initialisiereHardware() {
    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, LOW);
    }
}

void initialisiereWlan() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    masterStatus.letzter_wlan_versuch_ms = millis();
    logf("INFO", "WLAN-Verbindung gestartet: SSID=%s", WIFI_SSID);
}

void pruefeWlanVerbindung() {
    const bool verbunden = (WiFi.status() == WL_CONNECTED);
    if (verbunden != masterStatus.wlan_verbunden) {
        masterStatus.wlan_verbunden = verbunden;
        if (verbunden) {
            logf("INFO", "WLAN verbunden: IP=%s Kanal=%d", WiFi.localIP().toString().c_str(), WiFi.channel());
        } else {
            logf("WARN", "WLAN getrennt");
        }
    }

    if (!verbunden && (millis() - masterStatus.letzter_wlan_versuch_ms) >= WIFI_RECONNECT_INTERVAL_MS) {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        masterStatus.letzter_wlan_versuch_ms = millis();
        logf("INFO", "WLAN-Reconnect gestartet");
    }
}

void initialisiereEspNow() {
    if (!masterStatus.wlan_verbunden) {
        esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    }

    if (esp_now_init() != ESP_OK) {
        masterStatus.espnow_bereit = false;
        logf("WARN", "ESP-NOW Initialisierung fehlgeschlagen");
        return;
    }

    esp_now_register_send_cb(onEspNowSent);
    esp_now_register_recv_cb(onEspNowReceive);
    masterStatus.espnow_bereit = true;
    logf("INFO", "ESP-NOW bereit");
}

void initialisiereMqtt() {
    mqttBrokerNutzeDirekteIp = mqttBrokerIp.fromString(MQTT_HOST);
    if (mqttBrokerNutzeDirekteIp) mqttClient.setServer(mqttBrokerIp, MQTT_PORT);
    else mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_BYTES);
    logf("INFO", "MQTT konfiguriert: %s:%d (typ=%s)", MQTT_HOST, MQTT_PORT, mqttBrokerTypText());
}

void pruefeMqttVerbindung() {
    if (!masterStatus.wlan_verbunden) {
        if (masterStatus.mqtt_verbunden) {
            mqttClient.disconnect();
            masterStatus.mqtt_verbunden = false;
        }
        return;
    }

    if (mqttClient.connected()) {
        mqttClient.loop();
        if (!masterStatus.mqtt_verbunden) masterStatus.mqtt_verbunden = true;
        return;
    }

    masterStatus.mqtt_verbunden = false;
    if ((millis() - masterStatus.letzter_mqtt_versuch_ms) < MQTT_RECONNECT_INTERVAL_MS) {
        return;
    }

    masterStatus.letzter_mqtt_versuch_ms = millis();

    char willTopic[96] = {0};
    char willPayload[192] = {0};
    baueMasterTopic("status", willTopic, sizeof(willTopic));
    baueMasterStatusJson(willPayload, sizeof(willPayload), false);

    const bool verbunden = mqttClient.connect(
        DEVICE_ID,
        MQTT_USER,
        MQTT_PASSWORD,
        willTopic,
        0,
        true,
        willPayload,
        true);

    if (!verbunden) {
        loggeMqttConnectFehler();
        return;
    }

    masterStatus.mqtt_verbunden = true;
    logf("INFO", "MQTT verbunden");

    if (!mqttClient.subscribe(MQTT_TOPIC_COMMAND_SUB)) {
        logf("WARN", "MQTT Subscribe fehlgeschlagen: %s", MQTT_TOPIC_COMMAND_SUB);
    }

    publishBekannteNodesNachReconnect();
}

void pruefePendingCmdTimeouts() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].pending_cmd.aktiv) continue;
        if ((millis() - nodeStates[i].pending_cmd.letztes_senden_ms) < COMMAND_ACK_TIMEOUT_MS) continue;

        if (nodeStates[i].pending_cmd.retries < COMMAND_MAX_RETRIES) {
            SmartHome::CmdPayload payload = {};
            payload.cmd_type = nodeStates[i].pending_cmd.cmd_type;
            payload.param1 = nodeStates[i].pending_cmd.param1;
            payload.param2 = nodeStates[i].pending_cmd.param2;
            if (sendePaketMitOptionen(
                    nodeStates[i].mac,
                    SH_MSG_CMD,
                    &payload,
                    sizeof(payload),
                    "COMMAND retry",
                    (uint8_t)(SH_FLAG_ACK_REQUEST | SH_FLAG_RETRANSMIT),
                    true,
                    nodeStates[i].pending_cmd.seq,
                    nullptr)) {
                nodeStates[i].pending_cmd.retries++;
                nodeStates[i].pending_cmd.letztes_senden_ms = millis();
                continue;
            }
        }

        publishNodeAck(i, nodeStates[i].pending_cmd.request_id, nodeStates[i].pending_cmd.command_channel, "timeout", (int)SH_ERROR_ACK_TIMEOUT, SH_MSG_CMD, nodeStates[i].pending_cmd.seq, "master_timeout");
        nodeStates[i].pending_cmd = {};
    }
}

void pruefePendingCfgTimeouts() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].pending_cfg.aktiv) continue;
        if ((millis() - nodeStates[i].pending_cfg.letztes_senden_ms) < COMMAND_ACK_TIMEOUT_MS) continue;

        if (nodeStates[i].pending_cfg.retries < COMMAND_MAX_RETRIES) {
            SmartHome::CfgPayload payload = {};
            payload.param_id = nodeStates[i].pending_cfg.param_id;
            payload.value = nodeStates[i].pending_cfg.value;
            if (sendePaketMitOptionen(
                    nodeStates[i].mac,
                    SH_MSG_CFG,
                    &payload,
                    sizeof(payload),
                    "CONFIG retry",
                    (uint8_t)(SH_FLAG_ACK_REQUEST | SH_FLAG_RETRANSMIT),
                    true,
                    nodeStates[i].pending_cfg.seq,
                    nullptr)) {
                nodeStates[i].pending_cfg.retries++;
                nodeStates[i].pending_cfg.letztes_senden_ms = millis();
                continue;
            }
        }

        publishNodeAck(i, nodeStates[i].pending_cfg.request_id, nodeStates[i].pending_cfg.command_channel, "timeout", (int)SH_ERROR_ACK_TIMEOUT, SH_MSG_CFG, nodeStates[i].pending_cfg.seq, "master_timeout");
        nodeStates[i].pending_cfg = {};
    }
}

void pruefeOfflineTimeout() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].online) continue;
        if ((millis() - nodeStates[i].letzter_kontakt_ms) > offlineTimeoutMsForPowerType(nodeStates[i].power_type)) {
            nodeStates[i].online = false;
            publishNodeAvailability(i);
            logf("WARN", "Node %s nicht mehr online (availability=%s)", nodeStates[i].device_id, availabilityStateText(i));
        }
    }
}

void gibStartmeldungAus() {
    if (!DEBUG_LOKAL_AKTIV) return;

    Serial.println("================================");
    Serial.println(PROJECT_NAME);
    Serial.print(DATEI_GERAET);
    Serial.print(" v");
    Serial.println(DATEI_VERSION);
    Serial.print("FW: ");
    Serial.println(PROJECT_VERSION);
    Serial.print("Variante: ");
    Serial.println(FW_VARIANT);
    Serial.println("Master-Stand:");
    Serial.println(" - dynamische Node-Registry");
    Serial.println(" - HELLO / HELLO_ACK / HEARTBEAT / STATE / EVENT / ACK");
    Serial.println(" - set_relay / get_state / set_config(report_interval_s)");
    Serial.println(" - cover: open / close / stop / set_position");
    Serial.println(" - Pending/ACK pro Geraet");
    Serial.print("Max. dynamische Nodes: ");
    Serial.println(MAX_DYNAMIC_NODES);
    Serial.println("================================");
}

}  // namespace

void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    masterStatus = {};
    initialisiereNodeStates();
    gibStartmeldungAus();
    initialisiereHardware();
    initialisiereWlan();
    delay(500);
    pruefeWlanVerbindung();
    initialisiereEspNow();
    initialisiereMqtt();
}

void loop() {
    pruefeWlanVerbindung();
    pruefeMqttVerbindung();
    pruefePendingCmdTimeouts();
    pruefePendingCfgTimeouts();
    pruefeOfflineTimeout();
    delay(LOOP_INTERVAL_MS);
}
