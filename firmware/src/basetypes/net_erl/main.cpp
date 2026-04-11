#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <string.h>

#include "NetErlProvisioning.h"
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

constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-ERL";
constexpr char DATEI_VERSION[] = "0.1.0";
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
constexpr uint32_t DEFAULT_REPORT_INTERVAL_S = NET_ERL_STATE_INTERVAL_MS / 1000UL;
constexpr uint32_t DEFAULT_SENSOR_SEND_INTERVAL_S = DEFAULT_REPORT_INTERVAL_S;
constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
static_assert(
    sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "NET_ERL_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");

struct NodeState {
    bool provisioning_bereit;
    bool setup_mode;
    bool setup_ap_aktiv;
    bool restart_pending;
    bool master_bekannt;
    bool master_mac_gueltig;
    bool state_report_offen;
    bool funk_bereit;
    bool relay_1;
    bool fault;
    unsigned long letztes_hello_ms;
    unsigned long letzter_heartbeat_ms;
    unsigned long letzter_state_ms;
    unsigned long restart_requested_at_ms;
    unsigned long state_interval_ms;
    uint32_t report_interval_s;
    uint32_t stored_sensor_send_interval_s;
    uint8_t master_mac[6];
    uint8_t naechste_seq;
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE];
};

NodeState nodeStatus = {};

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

void provisioningLog(const char* level, const char* message) {
    if (!DEBUG_LOKAL_AKTIV || level == nullptr || message == nullptr) return;

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

bool senderIstProvisionierterMaster(const uint8_t* senderMac) {
    return nodeStatus.master_mac_gueltig &&
           senderMac != nullptr &&
           memcmp(senderMac, nodeStatus.master_mac, sizeof(nodeStatus.master_mac)) == 0;
}

const uint8_t* holeHelloZielMac() {
    return nodeStatus.master_mac_gueltig ? nodeStatus.master_mac : BROADCAST_MAC;
}

void wendeReportIntervalAn(uint32_t wertS) {
    nodeStatus.report_interval_s = wertS;
    nodeStatus.state_interval_ms = (unsigned long)nodeStatus.report_interval_s * 1000UL;
}

class NetErlProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
  public:
    const char* pageTitle() const override { return "NET-ERL Provisioning"; }
    const char* pageIntro() const override { return "Node-Basis fuer Master-Bindung und Statusintervall."; }
    const char* deviceSectionTitle() const override { return "NET-ERL-Spezifisch"; }
    const char* deviceSectionIntro() const override {
        return "Dieser Basispfad hat aktuell keine zusaetzlichen lokalen Setup-Werte.";
    }

    void loadDeviceDefaults() override {}
    bool loadDeviceSettings(Preferences& /*prefs*/) override { return false; }
    bool saveDeviceSettings(Preferences& /*prefs*/) override { return true; }
    bool clearDeviceSettings(Preferences& /*prefs*/) override { return true; }
    void captureDeviceSnapshot() override {}
    void restoreDeviceSnapshot() override {}
    bool parseDeviceSave(WebServer& /*server*/, String& /*errorText*/) override { return true; }
    void applyParsedDeviceSettings() override {}
    void discardParsedDeviceSettings() override {}
    void appendDeviceFieldsHtml(String& /*page*/, WebServer* /*sourceServer*/) const override {}
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetErlProvisioningHandler netErlProvisioningHandler;

bool speichereReportIntervalMitRollback(uint32_t valueS) {
    if (!nodeProvisioning.isSendIntervalValid(valueS)) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    nodeProvisioning.captureBasisSnapshot(basisSnapshot);

    wendeReportIntervalAn(valueS);
    nodeStatus.state_report_offen = true;

    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }

    nodeProvisioning.restoreBasisSnapshot(basisSnapshot);
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    return false;
}

void setzeRelayAusgang(bool an) {
    digitalWrite(PIN_RELAY_1, (an == RELAY_1_ACTIVE_HIGH) ? HIGH : LOW);

#if PIN_STATUS_LED >= 0
    digitalWrite(PIN_STATUS_LED, an ? HIGH : LOW);
#endif

    logf("INFO", "GPIO%d relay_1 -> %s", PIN_RELAY_1, an ? "HIGH" : "LOW");
}

bool stellePeerSicher(const uint8_t* mac) {
    if (!nodeStatus.funk_bereit || mac == nullptr) return false;
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
    if (!nodeStatus.funk_bereit || zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t seq = nodeStatus.naechste_seq++;
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
    copyText(payload.sensor_mask, sizeof(payload.sensor_mask), "XXXXXXXXXX");
    copyText(payload.input_mask, sizeof(payload.input_mask), "XXXXX");

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

    SmartHome::StateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.relay_1 = nodeStatus.relay_1 ? 1U : 0U;
    payload.fault = nodeStatus.fault ? 1U : 0U;

    if (!sendePaket(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    nodeStatus.state_report_offen = false;
    nodeStatus.letzter_state_ms = millis();
    return true;
}

bool sendeRelayEvent(uint8_t trigger) {
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = SH_EVENT_RELAY_CHANGED;
    payload.trigger = trigger;
    payload.param1 = nodeStatus.relay_1 ? 1U : 0U;
    payload.param2 = 0U;

    return sendePaket(nodeStatus.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT_RELAY_CHANGED");
}

void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    if (!nodeStatus.master_mac_gueltig) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return;
    }

    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    nodeStatus.master_bekannt = true;
    nodeStatus.state_report_offen = true;
    stellePeerSicher(nodeStatus.master_mac);
    logf("INFO", "HELLO_ACK empfangen");
}

bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    if (payload.param_id != SH_CFG_REPORT_INTERVAL_S) return false;

    const bool ok = speichereReportIntervalMitRollback(payload.value);
    if (!ok) {
        logf("WARN", "report_interval_s konnte nicht uebernommen werden");
    }
    return ok;
}

void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CFG ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    const bool ok = uebernehmeCfg(payload);
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        nodeStatus.state_report_offen = true;
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

        nodeStatus.relay_1 = neuerZustand;
        setzeRelayAusgang(nodeStatus.relay_1);
        nodeStatus.state_report_offen = true;
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
    if (nodeStatus.funk_bereit || nodeStatus.setup_mode) return;

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
    nodeStatus.funk_bereit = true;
    stellePeerSicher(BROADCAST_MAC);
    if (nodeStatus.master_mac_gueltig) {
        stellePeerSicher(nodeStatus.master_mac);
    }
}

void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    nodeStatus = {};
    nodeStatus.report_interval_s = DEFAULT_REPORT_INTERVAL_S;
    nodeStatus.stored_sensor_send_interval_s = DEFAULT_SENSOR_SEND_INTERVAL_S;
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    nodeStatus.state_report_offen = true;
    nodeStatus.relay_1 = false;
    nodeStatus.fault = false;

    pinMode(PIN_RELAY_1, OUTPUT);
    setzeRelayAusgang(false);

#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif

    const SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig =
        SmartHome::NetErlProvisioning::makeConfig(
            DEVICE_ID,
            DEFAULT_REPORT_INTERVAL_S,
            DEFAULT_SENSOR_SEND_INTERVAL_S,
            MIN_REPORT_INTERVAL_S,
            MAX_REPORT_INTERVAL_S);

    nodeStatus.provisioning_bereit = nodeProvisioning.begin(
        provisioningConfig,
        &nodeStatus.master_mac_gueltig,
        nodeStatus.master_mac,
        &nodeStatus.report_interval_s,
        &nodeStatus.stored_sensor_send_interval_s,
        &nodeStatus.setup_mode,
        &nodeStatus.setup_ap_aktiv,
        &nodeStatus.restart_pending,
        &nodeStatus.restart_requested_at_ms,
        nodeStatus.setup_ap_ssid,
        sizeof(nodeStatus.setup_ap_ssid),
        &netErlProvisioningHandler,
        provisioningLog);

    if (!nodeStatus.provisioning_bereit) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
        return;
    }

    wendeReportIntervalAn(nodeProvisioning.sanitizeStatusSendInterval(nodeStatus.report_interval_s));
    nodeStatus.stored_sensor_send_interval_s =
        nodeProvisioning.sanitizeSensorSendInterval(nodeStatus.stored_sensor_send_interval_s);

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO", "config report_interval_s=%lu stored_sensor_send_interval_s=%lu",
         (unsigned long)nodeStatus.report_interval_s,
         (unsigned long)nodeStatus.stored_sensor_send_interval_s);

    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        nodeProvisioning.enterSetupMode();
        return;
    }

    initialisiereFunk();
    sendeHello();
}

void loop() {
    nodeProvisioning.update();

    if (!nodeStatus.provisioning_bereit || nodeStatus.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    if (!nodeStatus.funk_bereit) {
        initialisiereFunk();
    }

    const unsigned long jetzt = millis();

    if (!nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendeHello();
    }

    if (nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
        sendeHeartbeat();
    }

    const bool stateFaellig =
        nodeStatus.master_bekannt &&
        nodeStatus.master_mac_gueltig &&
        (nodeStatus.state_report_offen ||
         (nodeStatus.state_interval_ms > 0UL &&
          (jetzt - nodeStatus.letzter_state_ms) >= nodeStatus.state_interval_ms));

    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
