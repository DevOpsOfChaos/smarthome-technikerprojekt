#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "AppConfig.h"
#include "PinConfig.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

#if NET_SEN_USE_BME280
  #include <Adafruit_BME280.h>
#endif

#if NET_SEN_USE_VEML7700
  #include <Adafruit_VEML7700.h>
#endif

constexpr bool DEBUG_LOKAL_AKTIV = DEVICE_DEBUG_AKTIV && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-SEN";
constexpr char DATEI_VERSION[] = "0.1.0";
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr uint16_t DEVICE_CAPS =
    (SENSOR_BME280_ENABLED ? (SH_CAP_TEMP | SH_CAP_HUM) : 0U) |
    (SENSOR_VEML7700_ENABLED ? SH_CAP_LUX : 0U);

struct SensorState {
    int16_t temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    bool fault;
};

struct NodeState {
    bool master_bekannt;
    bool master_mac_gueltig;
    bool state_report_offen;
    bool bme280_bereit;
    bool veml7700_bereit;
    unsigned long letztes_hello_ms;
    unsigned long letzter_heartbeat_ms;
    unsigned long letzter_state_ms;
    unsigned long letzter_sensor_poll_ms;
    unsigned long state_interval_ms;
    uint8_t master_mac[6];
    uint8_t naechste_seq;
    SensorState sensor;
};

NodeState nodeStatus = {};

#if NET_SEN_USE_BME280
Adafruit_BME280 sensorBme280;
#endif

#if NET_SEN_USE_VEML7700
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();
#endif

void logf(const char* level, const char* format, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;

    char message[224];
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

uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return a > b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

int16_t absDiffI16(int16_t a, int16_t b) {
    return a > b ? (int16_t)(a - b) : (int16_t)(b - a);
}

uint16_t clampToU16(long value) {
    if (value < 0L) return 0U;
    if (value > 65535L) return 65535U;
    return (uint16_t)value;
}

bool istBroadcastMac(const uint8_t* mac) {
    return mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0;
}

const uint8_t* holeHelloZielMac() {
    return nodeStatus.master_mac_gueltig ? nodeStatus.master_mac : BROADCAST_MAC;
}

void buildSensorMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXXXXXXX");
    if (targetSize < SH_SENSOR_MASK_LEN) return;

    target[0] = (DEVICE_CAPS & SH_CAP_TEMP) ? 'T' : 'X';
    target[1] = (DEVICE_CAPS & SH_CAP_HUM) ? 'H' : 'X';
    target[2] = (DEVICE_CAPS & SH_CAP_LUX) ? 'L' : 'X';
}

void buildInputMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXX");
}

bool stellePeerSicher(const uint8_t* mac) {
    if (mac == nullptr) return false;
    if (!istBroadcastMac(mac) && !SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = (uint8_t)WLAN_KANAL;
    peerInfo.encrypt = false;

    const esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        logf("WARN", "Peer konnte nicht angelegt werden (err=%d)", (int)err);
        return false;
    }

    return true;
}

bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    if (zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    SmartHome::fillHeader(header, msgType, nodeStatus.naechste_seq++, 0, (uint16_t)payloadLen);

    uint8_t* payloadBuffer = packet + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(packet, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, packet, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label, (int)err);
        return false;
    }

    return true;
}

bool sendeAck(const uint8_t* zielMac, uint8_t ackSeq, uint8_t ackMsgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = ackSeq;
    payload.ack_msg_type = ackMsgType;
    payload.status = status;
    return sendePaket(zielMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

bool sendeHello() {
    SmartHome::HelloPayload payload = {};
    char sensorMask[SH_SENSOR_MASK_LEN] = {0};
    char inputMask[SH_INPUT_MASK_LEN] = {0};

    buildSensorMask(sensorMask, sizeof(sensorMask));
    buildInputMask(inputMask, sizeof(inputMask));

    copyText(payload.device_id, sizeof(payload.device_id), DEVICE_ID);
    copyText(payload.device_name, sizeof(payload.device_name), DEVICE_NAME);
    payload.device_class = SH_CLASS_NET_SEN;
    payload.caps_hi = (uint8_t)((DEVICE_CAPS >> 8) & 0xFFU);
    payload.caps_lo = (uint8_t)(DEVICE_CAPS & 0xFFU);
    payload.power_type = SH_POWER_MAINS;
    payload.fw_version = 1U;
    payload.boot_counter = 1U;
    payload.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    payload.control_mode = DEVICE_CONTROL_MODE;
    payload.config_profile = DEVICE_CONFIG_PROFILE;
    payload.reporting_mode = DEVICE_REPORTING_MODE;
    copyText(payload.sensor_mask, sizeof(payload.sensor_mask), sensorMask);
    copyText(payload.input_mask, sizeof(payload.input_mask), inputMask);

    nodeStatus.letztes_hello_ms = millis();
    return sendePaket(holeHelloZielMac(), SH_MSG_HELLO, &payload, sizeof(payload), "HELLO");
}

bool sendeHeartbeat() {
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::HeartbeatPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.uptime_s = millis() / 1000UL;

    if (!sendePaket(nodeStatus.master_mac, SH_MSG_HEARTBEAT, &payload, sizeof(payload), "HEARTBEAT")) {
        return false;
    }

    nodeStatus.letzter_heartbeat_ms = millis();
    return true;
}

bool sendeState() {
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::SensorConfigStateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.temp_01c = nodeStatus.sensor.temp_01c;
    payload.hum_01pct = nodeStatus.sensor.hum_01pct;
    payload.lux = nodeStatus.sensor.lux;
    payload.motion = 0U;
    payload.fault = nodeStatus.sensor.fault ? 1U : 0U;
    payload.report_interval_s = (uint16_t)(nodeStatus.state_interval_ms / 1000UL);

    if (!sendePaket(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    nodeStatus.state_report_offen = false;
    nodeStatus.letzter_state_ms = millis();
    return true;
}

void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    memcpy(nodeStatus.master_mac, senderMac, sizeof(nodeStatus.master_mac));
    nodeStatus.master_mac_gueltig = true;
    nodeStatus.master_bekannt = true;
    nodeStatus.state_report_offen = true;
    stellePeerSicher(nodeStatus.master_mac);
    logf("INFO", "HELLO_ACK empfangen");
}

void verarbeiteCmd(const SmartHome::CmdPayload& payload) {
    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        nodeStatus.state_report_offen = true;
    }
}

bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    if (payload.param_id != SH_CFG_REPORT_INTERVAL_S) return false;
    if (payload.value < MIN_REPORT_INTERVAL_S || payload.value > MAX_REPORT_INTERVAL_S) return false;

    nodeStatus.state_interval_ms = (unsigned long)payload.value * 1000UL;
    nodeStatus.state_report_offen = true;
    return true;
}

void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    const bool ok = uebernehmeCfg(payload);
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* data, int len) {
    if (!senderMac || !data || len < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(data, (size_t)len)) return;

    const SmartHome::MsgHeader* header = reinterpret_cast<const SmartHome::MsgHeader*>(data);
    const uint8_t* payload = data + SH_HEADER_SIZE;

    switch (header->msg_type) {
        case SH_MSG_HELLO_ACK:
            if (header->payload_len == sizeof(SmartHome::HelloAckPayload)) {
                verarbeiteHelloAck(senderMac, *reinterpret_cast<const SmartHome::HelloAckPayload*>(payload));
            }
            break;

        case SH_MSG_CMD:
            if (header->payload_len == sizeof(SmartHome::CmdPayload)) {
                verarbeiteCmd(*reinterpret_cast<const SmartHome::CmdPayload*>(payload));
            }
            break;

        case SH_MSG_CFG:
            if (header->payload_len == sizeof(SmartHome::CfgPayload)) {
                verarbeiteCfg(senderMac, *header, *reinterpret_cast<const SmartHome::CfgPayload*>(payload));
            }
            break;

        default:
            break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!info) return;
    verarbeiteEspNowPaket(info->src_addr, data, len);
}
#else
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* data, int len) {
    verarbeiteEspNowPaket(senderMac, data, len);
}
#endif

void onEspNowSend(const uint8_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

void initialisiereFunk() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);

    const esp_err_t kanalErr = esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (kanalErr != ESP_OK) {
        logf("WARN", "WLAN-Kanal %d konnte nicht gesetzt werden (err=%d)", WLAN_KANAL, (int)kanalErr);
    }

    if (esp_now_init() != ESP_OK) {
        logf("WARN", "ESP-NOW Initialisierung fehlgeschlagen");
        return;
    }

    esp_now_register_send_cb(onEspNowSend);
    esp_now_register_recv_cb(onEspNowReceive);
    stellePeerSicher(BROADCAST_MAC);
}

void initialisiereSensorik() {
    nodeStatus.bme280_bereit = false;
    nodeStatus.veml7700_bereit = false;

    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);

#if NET_SEN_USE_BME280
    nodeStatus.bme280_bereit = sensorBme280.begin((uint8_t)SENSOR_BME280_ADDRESS, &Wire);
    if (!nodeStatus.bme280_bereit) {
        logf("WARN", "BME280 nicht gefunden (addr=0x%02X)", SENSOR_BME280_ADDRESS);
    }
#endif

#if NET_SEN_USE_VEML7700
    nodeStatus.veml7700_bereit = sensorVeml7700.begin();
    if (!nodeStatus.veml7700_bereit) {
        logf("WARN", "VEML7700 nicht gefunden");
    }
#endif

    nodeStatus.sensor.fault = !nodeStatus.bme280_bereit || !nodeStatus.veml7700_bereit;
}

void pollSensorik() {
    const unsigned long jetzt = millis();
    if ((jetzt - nodeStatus.letzter_sensor_poll_ms) < SENSOR_READ_INTERVAL_MS) return;
    nodeStatus.letzter_sensor_poll_ms = jetzt;

    SensorState neuerState = nodeStatus.sensor;
    bool fault = false;

#if NET_SEN_USE_BME280
    if (!nodeStatus.bme280_bereit) {
        fault = true;
    } else {
        const float temp = sensorBme280.readTemperature();
        const float hum = sensorBme280.readHumidity();
        if (!isfinite(temp) || !isfinite(hum)) {
            fault = true;
        } else {
            neuerState.temp_01c = (int16_t)lroundf(temp * 10.0f);
            neuerState.hum_01pct = clampToU16((long)lroundf(hum * 10.0f));
        }
    }
#endif

#if NET_SEN_USE_VEML7700
    if (!nodeStatus.veml7700_bereit) {
        fault = true;
    } else {
        const float lux = sensorVeml7700.readLux();
        if (!isfinite(lux) || lux < 0.0f) {
            fault = true;
        } else {
            neuerState.lux = clampToU16((long)lroundf(lux));
        }
    }
#endif

    neuerState.fault = fault;

    const bool wesentlichGeaendert =
        absDiffI16(neuerState.temp_01c, nodeStatus.sensor.temp_01c) >= TEMP_DELTA_01C ||
        absDiffU16(neuerState.hum_01pct, nodeStatus.sensor.hum_01pct) >= HUM_DELTA_01PCT ||
        absDiffU16(neuerState.lux, nodeStatus.sensor.lux) >= LUX_DELTA ||
        neuerState.fault != nodeStatus.sensor.fault;

    nodeStatus.sensor = neuerState;
    if (wesentlichGeaendert) {
        nodeStatus.state_report_offen = true;
    }
}

void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    nodeStatus = {};
    nodeStatus.state_interval_ms = STATE_INTERVAL_MS;
    nodeStatus.state_report_offen = true;
    nodeStatus.sensor.fault = true;

    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, LOW);
    }

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);

    initialisiereSensorik();
    initialisiereFunk();
    sendeHello();
}

void loop() {
    const unsigned long jetzt = millis();

    pollSensorik();

    if (!nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendeHello();
    }

    if (nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
        sendeHeartbeat();
    }

    const bool stateFaellig =
        nodeStatus.master_mac_gueltig &&
        (nodeStatus.state_report_offen ||
         (nodeStatus.state_interval_ms > 0UL &&
          (jetzt - nodeStatus.letzter_state_ms) >= nodeStatus.state_interval_ms));

    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
