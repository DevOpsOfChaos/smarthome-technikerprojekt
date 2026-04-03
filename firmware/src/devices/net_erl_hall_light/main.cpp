#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <DHT.h>
#include <Adafruit_VEML7700.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-ERL";
constexpr char DATEI_VERSION[] = "0.4.0";
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr char DEVICE_ID[] = NET_ERL_DEVICE_ID;
constexpr char DEVICE_NAME[] = NET_ERL_DEVICE_NAME;
constexpr char FW_VARIANT[] = NET_ERL_FW_VARIANT;
constexpr uint16_t DEVICE_CAPS = (uint16_t)NET_ERL_DEVICE_CAPS;
constexpr uint8_t DEVICE_CONTROL_MODE = NET_ERL_DEVICE_CONTROL_MODE;
constexpr uint8_t DEVICE_CONFIG_PROFILE = NET_ERL_DEVICE_CONFIG_PROFILE;
constexpr uint8_t DEVICE_REPORTING_MODE = NET_ERL_DEVICE_REPORTING_MODE;
constexpr uint8_t DEVICE_META_SCHEMA_VERSION = SH_META_SCHEMA_VERSION_CURRENT;

constexpr int WLAN_KANAL = NET_ERL_WLAN_CHANNEL;
constexpr unsigned long HELLO_RETRY_INTERVAL_MS = NET_ERL_HELLO_RETRY_INTERVAL_MS;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = NET_ERL_HEARTBEAT_INTERVAL_MS;
constexpr unsigned long LOOP_INTERVAL_MS = NET_ERL_LOOP_INTERVAL_MS;
constexpr uint16_t MIN_REPORT_INTERVAL_S = NET_ERL_MIN_REPORT_INTERVAL_S;
constexpr uint16_t MAX_REPORT_INTERVAL_S = NET_ERL_MAX_REPORT_INTERVAL_S;
constexpr uint32_t BOOT_COUNTER = NET_ERL_BOOT_COUNTER;

DHT dht(PIN_DHT22, DHT22);
Adafruit_VEML7700 veml = Adafruit_VEML7700();

struct SensorState {
    int16_t temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    bool motion;
    bool fault;
};

struct HallRuntime {
    bool motion_aktiv;
    bool relay_1;
    bool fault;
    bool dht_ok;
    bool lux_ok;
    bool pir_raw;
    bool relay_auto_owned;
    bool blocked_by_lux;
    bool pending_auto_on_decision;
    uint8_t pending_motion_event_state;
    unsigned long letztes_hello_ms;
    unsigned long letzter_heartbeat_ms;
    unsigned long letzter_state_ms;
    unsigned long letztes_sensor_poll_ms;
    unsigned long letztes_env_sample_ms;
    unsigned long letztes_snapshot_log_ms;
    unsigned long motion_deadline_ms;
    unsigned long state_interval_ms;
    uint8_t master_mac[6];
    uint8_t naechste_seq;
    bool master_bekannt;
    bool master_mac_gueltig;
    bool state_report_offen;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
    uint16_t auto_off_delay_s;
    SensorState sensor;
};

HallRuntime runtime = {};

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

bool istBroadcastMac(const uint8_t* mac) {
    return mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0;
}

const uint8_t* holeHelloZielMac() {
    return runtime.master_mac_gueltig ? runtime.master_mac : BROADCAST_MAC;
}

void setzeRelayAusgang(bool an, const char* grund) {
#if RELAY_1_ACTIVE_HIGH
    digitalWrite(PIN_RELAY_1, an ? HIGH : LOW);
#else
    digitalWrite(PIN_RELAY_1, an ? LOW : HIGH);
#endif

#if PIN_STATUS_LED >= 0
    digitalWrite(PIN_STATUS_LED, an ? HIGH : LOW);
#endif

    runtime.relay_1 = an;
    logf("INFO", "GPIO%d relay_1 -> %s (%s)", PIN_RELAY_1, an ? "HIGH" : "LOW", grund ? grund : "unbekannt");
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

bool sendePaketMitOptionen(
    const uint8_t* zielMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label,
    uint8_t flags,
    uint8_t* verwendeteSeq)
{
    if (zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t seq = runtime.naechste_seq++;
    SmartHome::fillHeader(header, msgType, seq, flags, (uint16_t)payloadLen);

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

    if (verwendeteSeq != nullptr) {
        *verwendeteSeq = seq;
    }
    return true;
}

bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    return sendePaketMitOptionen(zielMac, msgType, payload, payloadLen, label, 0U, nullptr);
}

bool sendeAck(const uint8_t* zielMac, uint8_t ackSeq, uint8_t ackMsgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = ackSeq;
    payload.ack_msg_type = ackMsgType;
    payload.status = status;
    return sendePaket(zielMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

uint8_t holeAutoFlags() {
    uint8_t flags = 0U;
    if (runtime.motion_aktiv) flags |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (runtime.lux_ok) flags |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    flags |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) flags |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) flags |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    return flags;
}

bool sendeHello() {
    SmartHome::HelloPayload payload = {};
    copyText(payload.device_id, sizeof(payload.device_id), DEVICE_ID);
    copyText(payload.device_name, sizeof(payload.device_name), DEVICE_NAME);
    payload.device_class = SH_CLASS_NET_ERL;
    payload.caps_hi = (uint8_t)((DEVICE_CAPS >> 8) & 0xFFU);
    payload.caps_lo = (uint8_t)(DEVICE_CAPS & 0xFFU);
    payload.power_type = SH_POWER_MAINS;
    payload.fw_version = 1U;
    payload.boot_counter = BOOT_COUNTER;
    payload.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    payload.control_mode = DEVICE_CONTROL_MODE;
    payload.config_profile = DEVICE_CONFIG_PROFILE;
    payload.reporting_mode = DEVICE_REPORTING_MODE;
    copyText(payload.sensor_mask, sizeof(payload.sensor_mask), "THLMXXXXXX");
    copyText(payload.input_mask, sizeof(payload.input_mask), "XXXXX");

    runtime.letztes_hello_ms = millis();
    return sendePaket(holeHelloZielMac(), SH_MSG_HELLO, &payload, sizeof(payload), "HELLO");
}

bool sendeHeartbeat() {
    if (!runtime.master_mac_gueltig) return false;

    SmartHome::HeartbeatPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.uptime_s = millis() / 1000UL;

    if (!sendePaket(runtime.master_mac, SH_MSG_HEARTBEAT, &payload, sizeof(payload), "HEARTBEAT")) {
        return false;
    }

    runtime.letzter_heartbeat_ms = millis();
    return true;
}

bool sendeState() {
    if (!runtime.master_mac_gueltig) return false;

    SmartHome::RelayComfortConfigStateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.relay_1 = runtime.relay_1 ? 1U : 0U;
    payload.temp_01c = runtime.sensor.temp_01c;
    payload.hum_01pct = runtime.sensor.hum_01pct;
    payload.lux = runtime.sensor.lux;
    payload.motion = runtime.motion_aktiv ? 1U : 0U;
    payload.auto_flags = holeAutoFlags();
    payload.fault = runtime.fault ? 1U : 0U;
    payload.report_interval_s = runtime.report_interval_s;
    payload.auto_on_lux_threshold = runtime.auto_on_lux_threshold;

    if (!sendePaket(runtime.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    runtime.state_report_offen = false;
    runtime.letzter_state_ms = millis();
    return true;
}

bool sendeMotionEvent(bool motionState) {
    if (!runtime.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = SH_EVENT_MOTION_DETECTED;
    payload.trigger = SH_TRIGGER_AUTO;
    payload.param1 = motionState ? 1U : 0U;
    payload.param2 = 0U;

    return sendePaket(runtime.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), motionState ? "EVENT_MOTION_TRUE" : "EVENT_MOTION_FALSE");
}

void sendeAusstehendesMotionEvent() {
    if (!runtime.master_mac_gueltig || runtime.pending_motion_event_state == 0U) return;

    const bool motionState = (runtime.pending_motion_event_state == 1U);
    if (sendeMotionEvent(motionState)) {
        runtime.pending_motion_event_state = 0U;
    }
}

bool sendeRelayEvent(uint8_t trigger) {
    if (!runtime.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = SH_EVENT_RELAY_CHANGED;
    payload.trigger = trigger;
    payload.param1 = runtime.relay_1 ? 1U : 0U;
    payload.param2 = 0U;

    return sendePaket(runtime.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT_RELAY_CHANGED");
}

void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    memcpy(runtime.master_mac, senderMac, sizeof(runtime.master_mac));
    runtime.master_mac_gueltig = true;
    runtime.master_bekannt = true;
    runtime.state_report_offen = true;
    stellePeerSicher(runtime.master_mac);
    sendeAusstehendesMotionEvent();
    logf("INFO", "HELLO_ACK empfangen");
}

bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    switch (payload.param_id) {
        case SH_CFG_REPORT_INTERVAL_S:
            if (payload.value < MIN_REPORT_INTERVAL_S || payload.value > MAX_REPORT_INTERVAL_S) return false;
            runtime.report_interval_s = payload.value;
            runtime.state_interval_ms = (unsigned long)runtime.report_interval_s * 1000UL;
            logf("INFO", "report_interval_s -> %u", (unsigned)runtime.report_interval_s);
            runtime.state_report_offen = true;
            return true;

        case SH_CFG_LIGHT_THRESHOLD_ON:
            runtime.auto_on_lux_threshold = payload.value;
            logf("INFO", "auto_on_lux_threshold -> %u", (unsigned)runtime.auto_on_lux_threshold);
            runtime.state_report_offen = true;
            return true;

        case SH_CFG_AUTO_OFF_DELAY_S:
            runtime.auto_off_delay_s = payload.value;
            logf("INFO", "auto_off_delay_s -> %u", (unsigned)runtime.auto_off_delay_s);
            runtime.state_report_offen = true;
            return true;

        default:
            return false;
    }
}

void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    const bool ok = uebernehmeCfg(payload);
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        runtime.state_report_offen = true;
        return;
    }

    if (payload.cmd_type == SH_CMD_SET_RELAY) {
        const bool gueltigerIndex = (payload.param1 == 0U);
        const bool neuerZustand = (payload.param2 != 0U);

        if (!gueltigerIndex) {
            if (header.flags & SH_FLAG_ACK_REQUEST) {
                sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
            }
            logf("WARN", "SET_RELAY verworfen: relay_index=%u", (unsigned)payload.param1);
            return;
        }

        runtime.relay_auto_owned = false;
        runtime.pending_auto_on_decision = false;
        setzeRelayAusgang(neuerZustand, "master_cmd");
        runtime.state_report_offen = true;
        sendeRelayEvent(SH_TRIGGER_MASTER_CMD);

        if (header.flags & SH_FLAG_ACK_REQUEST) {
            sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_OK);
        }
        return;
    }

    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
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
                verarbeiteCmd(senderMac, *header, *reinterpret_cast<const SmartHome::CmdPayload*>(payload));
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
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
    dht.begin();

    runtime.dht_ok = true;
    if (!veml.begin()) {
        runtime.lux_ok = false;
        logf("WARN", "VEML7700 Init fehlgeschlagen");
    } else {
        runtime.lux_ok = true;
        veml.setGain(VEML7700_GAIN_1);
        veml.setIntegrationTime(VEML7700_IT_100MS);
        logf("INFO", "VEML7700 bereit");
    }

    pinMode(PIN_PIR, INPUT);
}

void setzeSensorDefaults() {
    runtime.sensor.temp_01c = INT16_MIN;
    runtime.sensor.hum_01pct = 0xFFFFU;
    runtime.sensor.lux = 0xFFFFU;
    runtime.sensor.motion = false;
    runtime.sensor.fault = false;
}

void leseUmweltsensoren(unsigned long jetzt) {
    if ((jetzt - runtime.letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    runtime.letztes_env_sample_ms = jetzt;

    const float temp = dht.readTemperature();
    const float hum = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
        runtime.dht_ok = false;
        logf("WARN", "DHT22 Read fehlgeschlagen");
    } else {
        runtime.dht_ok = true;
        runtime.sensor.temp_01c = (int16_t)lroundf(temp * 10.0f);
        runtime.sensor.hum_01pct = (uint16_t)lroundf(hum * 10.0f);
    }

    if (runtime.lux_ok) {
        const float lux = veml.readLux();
        if (!isnan(lux) && lux >= 0.0f) {
            runtime.sensor.lux = (uint16_t)lroundf(lux);
        } else {
            runtime.lux_ok = false;
            logf("WARN", "VEML7700 Read fehlgeschlagen");
        }
    }

    runtime.fault = !(runtime.dht_ok && runtime.lux_ok);
    runtime.sensor.fault = runtime.fault;

    if (runtime.motion_aktiv &&
        runtime.pending_auto_on_decision &&
        !runtime.relay_1 &&
        runtime.sensor.lux != 0xFFFFU) {
        runtime.pending_auto_on_decision = false;
        if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true;
            runtime.blocked_by_lux = false;
            setzeRelayAusgang(true, "auto_on_motion_late_lux");
            sendeRelayEvent(SH_TRIGGER_AUTO);
            runtime.state_report_offen = true;
            logf("INFO", "motion weiter aktiv, auto_on nach erstem lux=%u <= schwelle=%u", (unsigned)runtime.sensor.lux, (unsigned)runtime.auto_on_lux_threshold);
        } else {
            runtime.blocked_by_lux = true;
            runtime.state_report_offen = true;
            logf("INFO", "motion weiter aktiv, auto_on nach erstem lux blockiert (lux=%u schwelle=%u)", (unsigned)runtime.sensor.lux, (unsigned)runtime.auto_on_lux_threshold);
        }
    }
}

void loggeSnapshot(unsigned long jetzt) {
    if ((jetzt - runtime.letztes_snapshot_log_ms) < NET_ERL_SNAPSHOT_LOG_INTERVAL_MS) return;
    runtime.letztes_snapshot_log_ms = jetzt;

    logf("INFO",
         "snapshot temp_01c=%d hum_01pct=%u lux=%u motion=%s relay_1=%s auto_owned=%s pending_auto_on=%s fault=%s pir_raw=%s",
         (int)runtime.sensor.temp_01c,
         (unsigned)runtime.sensor.hum_01pct,
         (unsigned)runtime.sensor.lux,
         runtime.motion_aktiv ? "true" : "false",
         runtime.relay_1 ? "true" : "false",
         runtime.relay_auto_owned ? "true" : "false",
         runtime.pending_auto_on_decision ? "true" : "false",
         runtime.fault ? "true" : "false",
         runtime.pir_raw ? "true" : "false");
}

void motionAktivWerden(unsigned long jetzt) {
    runtime.motion_aktiv = true;
    runtime.sensor.motion = true;
    runtime.motion_deadline_ms = jetzt + ((unsigned long)runtime.auto_off_delay_s * 1000UL);
    runtime.state_report_offen = true;
    runtime.pending_motion_event_state = 1U;
    sendeAusstehendesMotionEvent();

    runtime.blocked_by_lux = false;
    runtime.pending_auto_on_decision = false;
    if (!runtime.relay_1) {
        if (runtime.sensor.lux == 0xFFFFU) {
            runtime.pending_auto_on_decision = true;
            logf("INFO", "motion erkannt, warte auf ersten gueltigen lux-wert vor auto_on");
        } else if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true;
            setzeRelayAusgang(true, "auto_on_motion");
            sendeRelayEvent(SH_TRIGGER_AUTO);
            logf("INFO", "motion erkannt, lux=%u <= schwelle=%u, timer gestartet", (unsigned)runtime.sensor.lux, (unsigned)runtime.auto_on_lux_threshold);
        } else {
            runtime.blocked_by_lux = true;
            logf("INFO", "motion erkannt, auto_on durch lux blockiert (lux=%u schwelle=%u)", (unsigned)runtime.sensor.lux, (unsigned)runtime.auto_on_lux_threshold);
        }
    } else {
        logf("INFO", "motion erkannt, timer gestartet");
    }
}

void motionTimerReset(unsigned long jetzt) {
    runtime.motion_deadline_ms = jetzt + ((unsigned long)runtime.auto_off_delay_s * 1000UL);
}

void motionInaktivWerden() {
    runtime.motion_aktiv = false;
    runtime.sensor.motion = false;
    runtime.motion_deadline_ms = 0UL;
    runtime.blocked_by_lux = false;
    runtime.pending_auto_on_decision = false;
    runtime.state_report_offen = true;
    runtime.pending_motion_event_state = 2U;
    sendeAusstehendesMotionEvent();
    logf("INFO", "motion false nach timerablauf");

    if (runtime.relay_1 && runtime.relay_auto_owned) {
        setzeRelayAusgang(false, "auto_off_timer");
        sendeRelayEvent(SH_TRIGGER_AUTO_OFF_TIMER);
        runtime.relay_auto_owned = false;
        logf("INFO", "lampe aus nach timerablauf");
    }
}

void pollPIR(unsigned long jetzt) {
    if ((jetzt - runtime.letztes_sensor_poll_ms) < NET_ERL_SENSOR_POLL_INTERVAL_MS) return;
    runtime.letztes_sensor_poll_ms = jetzt;

    const bool pirHigh = (digitalRead(PIN_PIR) == HIGH);
    runtime.pir_raw = pirHigh;

    if (pirHigh) {
        if (!runtime.motion_aktiv) {
            motionAktivWerden(jetzt);
        } else {
            motionTimerReset(jetzt);
        }
        return;
    }

    if (runtime.motion_aktiv && runtime.motion_deadline_ms > 0UL && jetzt >= runtime.motion_deadline_ms) {
        motionInaktivWerden();
    }
}

void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    runtime = {};
    runtime.report_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
    runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    runtime.state_interval_ms = (unsigned long)runtime.report_interval_s * 1000UL;
    runtime.state_report_offen = true;

    setzeSensorDefaults();

    pinMode(PIN_RELAY_1, OUTPUT);
    setzeRelayAusgang(false, "boot_default_off");

#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO", "config report_interval_s=%u auto_off_delay_s=%u lux_threshold=%u",
         (unsigned)runtime.report_interval_s,
         (unsigned)runtime.auto_off_delay_s,
         (unsigned)runtime.auto_on_lux_threshold);

    initialisiereSensorik();
    initialisiereFunk();
    sendeHello();
}

void loop() {
    const unsigned long jetzt = millis();

    leseUmweltsensoren(jetzt);
    pollPIR(jetzt);
    loggeSnapshot(jetzt);
    sendeAusstehendesMotionEvent();

    if (!runtime.master_bekannt &&
        (jetzt - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendeHello();
    }

    if (runtime.master_bekannt &&
        (jetzt - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
        sendeHeartbeat();
    }

    const bool stateFaellig =
        runtime.master_mac_gueltig &&
        (runtime.state_report_offen ||
         (runtime.state_interval_ms > 0UL &&
          (jetzt - runtime.letzter_state_ms) >= runtime.state_interval_ms));

    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
