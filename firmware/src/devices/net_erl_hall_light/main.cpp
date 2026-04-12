#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "../../basetypes/net_erl/NetErlProvisioning.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-ERL";
constexpr char DATEI_VERSION[] = "0.5.0";
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
constexpr unsigned long SENSOR_RECOVERY_RETRY_INTERVAL_MS = NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS;
constexpr uint32_t MIN_REPORT_INTERVAL_S = NET_ERL_MIN_REPORT_INTERVAL_S;
constexpr uint32_t MAX_REPORT_INTERVAL_S = NET_ERL_MAX_REPORT_INTERVAL_S;
constexpr uint32_t DEFAULT_STORED_SENSOR_SEND_INTERVAL_S = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
constexpr uint32_t BOOT_COUNTER_PROTOCOL_PLACEHOLDER = NET_ERL_BOOT_COUNTER;

constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
constexpr const char* NET_ERL_HALL_STORAGE_NAMESPACE = "net_erl_hl";
constexpr const char* STORAGE_KEY_HALL_SETUP = "hall_setup_v1";
static_assert(
    sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "NET_ERL_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");
constexpr uint32_t NET_ERL_HALL_SETUP_MAGIC = 0x484C4C31UL;
constexpr uint16_t NET_ERL_HALL_SETUP_VERSION = 1U;

Adafruit_BME280 bme280;
Adafruit_VEML7700 veml = Adafruit_VEML7700();

struct SensorState {
    int16_t temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    bool motion;
    bool fault;
};

struct HallRuntime {
    bool provisioning_bereit;
    bool setup_mode;
    bool setup_ap_aktiv;
    bool restart_pending;
    bool funk_bereit;
    bool motion_aktiv;
    bool relay_1;
    bool fault;
    bool bme_ok;
    bool lux_ok;
    bool pir_raw;
    bool relay_auto_owned;
    bool blocked_by_lux;
    bool pending_auto_on_decision;
    uint8_t pending_motion_event_state;
    unsigned long restart_requested_at_ms;
    unsigned long letztes_hello_ms;
    unsigned long letzter_heartbeat_ms;
    unsigned long letzter_state_ms;
    unsigned long letztes_sensor_poll_ms;
    unsigned long letztes_env_sample_ms;
    unsigned long letzter_bme_recovery_ms;
    unsigned long letzter_lux_recovery_ms;
    unsigned long letztes_snapshot_log_ms;
    unsigned long motion_deadline_ms;
    unsigned long state_interval_ms;
    uint8_t master_mac[6];
    uint8_t naechste_seq;
    bool master_bekannt;
    bool master_mac_gueltig;
    bool state_report_offen;
    uint32_t report_interval_s;
    // Wird von der gemeinsamen Node-Basis gespeichert;
    // Hall-Light nutzt fuer STATE report_interval_s.
    uint32_t stored_sensor_send_interval_s;
    uint16_t auto_on_lux_threshold;
    uint16_t auto_off_delay_s;
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE];
    SensorState sensor;
};

struct HallPersistedSetupData {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint16_t autoOnLuxThreshold;
    uint16_t autoOffDelayS;
};

struct HallProvisioningSnapshot {
    uint16_t auto_on_lux_threshold;
    uint16_t auto_off_delay_s;
};

HallRuntime runtime = {};

bool parseUIntValue(const char* text, uint32_t& outValue) {
    if (text == nullptr || *text == '\0') return false;

    uint32_t parsed = 0UL;
    for (const char* current = text; *current != '\0'; ++current) {
        if (*current < '0' || *current > '9') {
            return false;
        }

        const uint32_t digit = (uint32_t)(*current - '0');
        if (parsed > ((0xFFFFFFFFUL - digit) / 10UL)) {
            return false;
        }
        parsed = (parsed * 10UL) + digit;
    }

    outValue = parsed;
    return true;
}

String htmlEscapeLocal(const String& text) {
    String escaped;
    escaped.reserve(text.length() + 16U);

    for (size_t i = 0U; i < text.length(); ++i) {
        const char current = text[i];
        switch (current) {
            case '&': escaped += F("&amp;"); break;
            case '<': escaped += F("&lt;"); break;
            case '>': escaped += F("&gt;"); break;
            case '"': escaped += F("&quot;"); break;
            case '\'': escaped += F("&#39;"); break;
            default: escaped += current; break;
        }
    }

    return escaped;
}

uint16_t clampToU16(long value) {
    if (value < 0L) return 0U;
    if (value > 65535L) return 65535U;
    return (uint16_t)value;
}

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

bool senderIstBekannterMaster(const uint8_t* senderMac) {
    return runtime.master_mac_gueltig &&
           senderMac != nullptr &&
           memcmp(senderMac, runtime.master_mac, sizeof(runtime.master_mac)) == 0;
}

const uint8_t* holeHelloZielMac() {
    return runtime.master_mac_gueltig ? runtime.master_mac : BROADCAST_MAC;
}

void wendeReportIntervalAn(uint32_t wertS) {
    runtime.report_interval_s = wertS;
    runtime.state_interval_ms = (unsigned long)runtime.report_interval_s * 1000UL;
}

void holeHallProvisioningSnapshot(HallProvisioningSnapshot& snapshot) {
    snapshot.auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    snapshot.auto_off_delay_s = runtime.auto_off_delay_s;
}

void wendeHallProvisioningSnapshotAn(const HallProvisioningSnapshot& snapshot) {
    runtime.auto_on_lux_threshold = snapshot.auto_on_lux_threshold;
    runtime.auto_off_delay_s = snapshot.auto_off_delay_s;
}

bool hallPersistenzdatenGueltig(const HallPersistedSetupData& data) {
    return data.magic == NET_ERL_HALL_SETUP_MAGIC &&
           data.version == NET_ERL_HALL_SETUP_VERSION;
}

HallPersistedSetupData baueHallPersistenzdatenAusRuntime() {
    HallPersistedSetupData data = {};
    data.magic = NET_ERL_HALL_SETUP_MAGIC;
    data.version = NET_ERL_HALL_SETUP_VERSION;
    data.autoOnLuxThreshold = runtime.auto_on_lux_threshold;
    data.autoOffDelayS = runtime.auto_off_delay_s;
    return data;
}

void wendeHallPersistenzdatenAn(const HallPersistedSetupData& data) {
    runtime.auto_on_lux_threshold = data.autoOnLuxThreshold;
    runtime.auto_off_delay_s = data.autoOffDelayS;
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

class NetErlHallProvisioningHandler final
    : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
  public:
    const char* pageTitle() const override { return "NET-ERL Hall Light Provisioning"; }
    const char* pageIntro() const override {
        return "Node-Basis oben. status_send_interval_s steuert Hall-Light-STATE; "
               "sensor_send_interval_s wird nur als Basisfeld mitgespeichert.";
    }
    const char* deviceSectionTitle() const override { return "Hall-Light-Spezifisch"; }
    const char* deviceSectionIntro() const override {
        return "Nur Lux-Schwelle und Nachlauf lokal provisionieren.";
    }

    void loadDeviceDefaults() override {
        runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
        runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    }

    bool loadDeviceSettings(Preferences& prefs) override {
        HallPersistedSetupData data = {};
        if (prefs.getBytesLength(STORAGE_KEY_HALL_SETUP) != sizeof(HallPersistedSetupData)) {
            return false;
        }

        if (prefs.getBytes(STORAGE_KEY_HALL_SETUP, &data, sizeof(data)) != sizeof(data)) {
            return false;
        }

        if (!hallPersistenzdatenGueltig(data)) {
            return false;
        }

        wendeHallPersistenzdatenAn(data);
        return true;
    }

    bool saveDeviceSettings(Preferences& prefs) override {
        const HallPersistedSetupData data = baueHallPersistenzdatenAusRuntime();
        return prefs.putBytes(STORAGE_KEY_HALL_SETUP, &data, sizeof(data)) == sizeof(data);
    }

    bool clearDeviceSettings(Preferences& prefs) override {
        prefs.remove(STORAGE_KEY_HALL_SETUP);
        return true;
    }

    void captureDeviceSnapshot() override { holeHallProvisioningSnapshot(snapshot_); }
    void restoreDeviceSnapshot() override { wendeHallProvisioningSnapshotAn(snapshot_); }

    bool parseDeviceSave(WebServer& server, String& errorText) override {
        pending_ = {};

        uint32_t autoOnLuxThreshold = 0UL;
        if (!parseUIntValue(server.arg("auto_on_lux_threshold").c_str(), autoOnLuxThreshold) ||
            autoOnLuxThreshold > 65535UL) {
            errorText = F("auto_on_lux_threshold ist ungueltig. Erlaubt sind 0 bis 65535.");
            return false;
        }

        uint32_t autoOffDelayS = 0UL;
        if (!parseUIntValue(server.arg("auto_off_delay_s").c_str(), autoOffDelayS) ||
            autoOffDelayS > 65535UL) {
            errorText = F("auto_off_delay_s ist ungueltig. Erlaubt sind 0 bis 65535.");
            return false;
        }

        pending_.auto_on_lux_threshold = (uint16_t)autoOnLuxThreshold;
        pending_.auto_off_delay_s = (uint16_t)autoOffDelayS;
        pending_.gueltig = true;
        return true;
    }

    void applyParsedDeviceSettings() override {
        if (!pending_.gueltig) return;
        runtime.auto_on_lux_threshold = pending_.auto_on_lux_threshold;
        runtime.auto_off_delay_s = pending_.auto_off_delay_s;
    }

    void discardParsedDeviceSettings() override { pending_ = {}; }

    void appendDeviceFieldsHtml(String& page, WebServer* sourceServer) const override {
        const String autoOnLuxThresholdText =
            sourceServer != nullptr && sourceServer->hasArg("auto_on_lux_threshold")
                ? sourceServer->arg("auto_on_lux_threshold")
                : String(runtime.auto_on_lux_threshold);
        const String autoOffDelayText =
            sourceServer != nullptr && sourceServer->hasArg("auto_off_delay_s")
                ? sourceServer->arg("auto_off_delay_s")
                : String(runtime.auto_off_delay_s);

        page += F("<div class=\"field\"><label for=\"auto_on_lux_threshold\">auto_on_lux_threshold</label>");
        page += F("<input id=\"auto_on_lux_threshold\" name=\"auto_on_lux_threshold\" type=\"number\" min=\"0\" max=\"65535\" step=\"1\" inputmode=\"numeric\" value=\"");
        page += htmlEscapeLocal(autoOnLuxThresholdText);
        page += F("\"><div class=\"hint\">Lux-Schwelle fuer automatisches Einschalten.</div></div>");

        page += F("<div class=\"field\"><label for=\"auto_off_delay_s\">auto_off_delay_s</label>");
        page += F("<input id=\"auto_off_delay_s\" name=\"auto_off_delay_s\" type=\"number\" min=\"0\" max=\"65535\" step=\"1\" inputmode=\"numeric\" value=\"");
        page += htmlEscapeLocal(autoOffDelayText);
        page += F("\"><div class=\"hint\">Nachlaufzeit nach letzter Bewegung in Sekunden.</div></div>");
    }

  private:
    struct PendingValues {
        bool gueltig;
        uint16_t auto_on_lux_threshold;
        uint16_t auto_off_delay_s;
    };

    PendingValues pending_ = {};
    HallProvisioningSnapshot snapshot_ = {};
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetErlHallProvisioningHandler netErlHallProvisioningHandler;

void holeSetupSnapshot(
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    HallProvisioningSnapshot& deviceSnapshot) {
    nodeProvisioning.captureBasisSnapshot(basisSnapshot);
    holeHallProvisioningSnapshot(deviceSnapshot);
}

void wendeSetupSnapshotAn(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const HallProvisioningSnapshot& deviceSnapshot) {
    nodeProvisioning.restoreBasisSnapshot(basisSnapshot);
    wendeHallProvisioningSnapshotAn(deviceSnapshot);
    wendeReportIntervalAn(runtime.report_interval_s);
}

bool speicherePersistenzMitRollback(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const HallProvisioningSnapshot& deviceSnapshot) {
    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }

    wendeSetupSnapshotAn(basisSnapshot, deviceSnapshot);
    return false;
}

bool stellePeerSicher(const uint8_t* mac) {
    if (!runtime.funk_bereit || mac == nullptr) return false;
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
    if (!runtime.funk_bereit || zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
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
    payload.boot_counter = BOOT_COUNTER_PROTOCOL_PLACEHOLDER;
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
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;

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
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;

    SmartHome::RelayComfortConfigStateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);

    // Hall-Light bleibt absichtlich beim kleinen Aussenvertrag.
    // BME280 kann Druck liefern, aber dieser Pfad meldet weiter nur Temperatur,
    // Feuchte, Lux, Motion und Relay nach oben.
    payload.relay_1 = runtime.relay_1 ? 1U : 0U;
    payload.temp_01c = runtime.sensor.temp_01c;
    payload.hum_01pct = runtime.sensor.hum_01pct;
    payload.lux = runtime.sensor.lux;
    payload.motion = runtime.motion_aktiv ? 1U : 0U;
    payload.auto_flags = holeAutoFlags();
    payload.fault = runtime.fault ? 1U : 0U;
    payload.report_interval_s = (uint16_t)runtime.report_interval_s;
    payload.auto_on_lux_threshold = runtime.auto_on_lux_threshold;

    if (!sendePaket(runtime.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    runtime.state_report_offen = false;
    runtime.letzter_state_ms = millis();
    return true;
}

bool sendeMotionEvent(bool motionState) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = SH_EVENT_MOTION_DETECTED;
    payload.trigger = SH_TRIGGER_AUTO;
    payload.param1 = motionState ? 1U : 0U;
    payload.param2 = 0U;

    return sendePaket(runtime.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), motionState ? "EVENT_MOTION_TRUE" : "EVENT_MOTION_FALSE");
}

void sendeAusstehendesMotionEvent() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig || runtime.pending_motion_event_state == 0U) return;

    const bool motionState = (runtime.pending_motion_event_state == 1U);
    if (sendeMotionEvent(motionState)) {
        runtime.pending_motion_event_state = 0U;
    }
}

bool sendeRelayEvent(uint8_t trigger) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;

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

    if (!runtime.master_mac_gueltig) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return;
    }

    if (!senderIstBekannterMaster(senderMac)) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    runtime.master_bekannt = true;
    runtime.state_report_offen = true;
    stellePeerSicher(runtime.master_mac);
    sendeAusstehendesMotionEvent();
    logf("INFO", "HELLO_ACK empfangen");
}

bool speichereReportIntervalAusCfg(uint32_t valueS) {
    if (!nodeProvisioning.isSendIntervalValid(valueS)) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    HallProvisioningSnapshot deviceSnapshot = {};
    holeSetupSnapshot(basisSnapshot, deviceSnapshot);

    wendeReportIntervalAn(valueS);
    runtime.state_report_offen = true;
    if (!speicherePersistenzMitRollback(basisSnapshot, deviceSnapshot)) {
        logf("WARN", "report_interval_s konnte nicht persistiert werden");
        return false;
    }

    return true;
}

bool speichereAutoOnLuxThresholdAusCfg(uint16_t value) {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    HallProvisioningSnapshot deviceSnapshot = {};
    holeSetupSnapshot(basisSnapshot, deviceSnapshot);

    runtime.auto_on_lux_threshold = value;
    runtime.state_report_offen = true;
    if (!speicherePersistenzMitRollback(basisSnapshot, deviceSnapshot)) {
        logf("WARN", "auto_on_lux_threshold konnte nicht persistiert werden");
        return false;
    }

    return true;
}

bool speichereAutoOffDelayAusCfg(uint16_t value) {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    HallProvisioningSnapshot deviceSnapshot = {};
    holeSetupSnapshot(basisSnapshot, deviceSnapshot);

    runtime.auto_off_delay_s = value;
    runtime.state_report_offen = true;
    if (!speicherePersistenzMitRollback(basisSnapshot, deviceSnapshot)) {
        logf("WARN", "auto_off_delay_s konnte nicht persistiert werden");
        return false;
    }

    return true;
}

bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    switch (payload.param_id) {
        case SH_CFG_REPORT_INTERVAL_S:
            if (!speichereReportIntervalAusCfg(payload.value)) return false;
            logf("INFO", "report_interval_s -> %u", (unsigned)runtime.report_interval_s);
            return true;

        case SH_CFG_LIGHT_THRESHOLD_ON:
            if (!speichereAutoOnLuxThresholdAusCfg(payload.value)) return false;
            logf("INFO", "auto_on_lux_threshold -> %u", (unsigned)runtime.auto_on_lux_threshold);
            return true;

        case SH_CFG_AUTO_OFF_DELAY_S:
            if (!speichereAutoOffDelayAusCfg(payload.value)) return false;
            logf("INFO", "auto_off_delay_s -> %u", (unsigned)runtime.auto_off_delay_s);
            return true;

        default:
            return false;
    }
}

void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    if (!senderIstBekannterMaster(senderMac)) {
        logf("WARN", "CFG ignoriert: Sender ist nicht der bekannte Master");
        return;
    }

    const bool ok = uebernehmeCfg(payload);
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    if (!senderIstBekannterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der bekannte Master");
        return;
    }

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

    // ESP-NOW-Callback und loop() teilen runtime; Handler deshalb kurz halten.
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
    if (runtime.funk_bereit || runtime.setup_mode) return;

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
    runtime.funk_bereit = true;
    stellePeerSicher(BROADCAST_MAC);
    if (runtime.master_mac_gueltig) {
        stellePeerSicher(runtime.master_mac);
    }
}

void konfiguriereVeml7700() {
    veml.setGain(VEML7700_GAIN_1);
    veml.setIntegrationTime(VEML7700_IT_100MS);
}

void initialisiereSensorik() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
    runtime.bme_ok = bme280.begin((uint8_t)NET_ERL_BME280_ADDRESS, &Wire);
    if (!runtime.bme_ok) {
        logf("WARN", "BME280 Init fehlgeschlagen (addr=0x%02X)", NET_ERL_BME280_ADDRESS);
    } else {
        logf("INFO", "BME280 bereit");
    }

    if (!veml.begin()) {
        runtime.lux_ok = false;
        logf("WARN", "VEML7700 Init fehlgeschlagen");
    } else {
        runtime.lux_ok = true;
        konfiguriereVeml7700();
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

bool sensorRecoveryFaellig(unsigned long letzterVersuchMs, unsigned long jetzt) {
    return letzterVersuchMs == 0UL ||
           (jetzt - letzterVersuchMs) >= SENSOR_RECOVERY_RETRY_INTERVAL_MS;
}

void versucheBmeRecovery(unsigned long jetzt) {
    if (runtime.bme_ok || !sensorRecoveryFaellig(runtime.letzter_bme_recovery_ms, jetzt)) return;

    runtime.letzter_bme_recovery_ms = jetzt;
    runtime.bme_ok = bme280.begin((uint8_t)NET_ERL_BME280_ADDRESS, &Wire);
    logf(runtime.bme_ok ? "INFO" : "WARN",
         runtime.bme_ok ? "BME280 Recovery erfolgreich" : "BME280 Recovery fehlgeschlagen");
}

void versucheLuxRecovery(unsigned long jetzt) {
    if (runtime.lux_ok || !sensorRecoveryFaellig(runtime.letzter_lux_recovery_ms, jetzt)) return;

    runtime.letzter_lux_recovery_ms = jetzt;
    runtime.lux_ok = veml.begin();
    if (runtime.lux_ok) {
        konfiguriereVeml7700();
    }
    logf(runtime.lux_ok ? "INFO" : "WARN",
         runtime.lux_ok ? "VEML7700 Recovery erfolgreich" : "VEML7700 Recovery fehlgeschlagen");
}

void leseUmweltsensoren(unsigned long jetzt) {
    if ((jetzt - runtime.letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    runtime.letztes_env_sample_ms = jetzt;

    versucheBmeRecovery(jetzt);
    versucheLuxRecovery(jetzt);

    bool bmeMesswertOk = runtime.bme_ok;
    if (!runtime.bme_ok) {
        bmeMesswertOk = false;
    } else {
        const float temp = bme280.readTemperature();
        const float hum = bme280.readHumidity();
        if (!isfinite(temp) || !isfinite(hum)) {
            bmeMesswertOk = false;
            runtime.bme_ok = false;
            logf("WARN", "BME280 Read fehlgeschlagen");
        } else {
            runtime.sensor.temp_01c = (int16_t)lroundf(temp * 10.0f);
            runtime.sensor.hum_01pct = clampToU16((long)lroundf(hum * 10.0f));
        }
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

    runtime.fault = !(bmeMesswertOk && runtime.lux_ok);
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
            // Sinkende Lux waehrend laufender Praesenz schaltet nicht automatisch nach.
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
    runtime.stored_sensor_send_interval_s = DEFAULT_STORED_SENSOR_SEND_INTERVAL_S;
    runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
    runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    runtime.state_report_offen = true;

    setzeSensorDefaults();

    pinMode(PIN_RELAY_1, OUTPUT);
    setzeRelayAusgang(false, "boot_default_off");

#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif

    SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig =
        SmartHome::NetErlProvisioning::makeConfig(
            DEVICE_ID,
            NET_ERL_HALL_STORAGE_NAMESPACE,
            NET_ERL_DEFAULT_REPORT_INTERVAL_S,
            DEFAULT_STORED_SENSOR_SEND_INTERVAL_S,
            MIN_REPORT_INTERVAL_S,
            MAX_REPORT_INTERVAL_S);
    provisioningConfig.setupButtonPin = SETUP_BUTTON_PIN;
    provisioningConfig.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    provisioningConfig.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS;
    provisioningConfig.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    provisioningConfig.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0;
    provisioningConfig.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    runtime.provisioning_bereit = nodeProvisioning.begin(
        provisioningConfig,
        &runtime.master_mac_gueltig,
        runtime.master_mac,
        &runtime.report_interval_s,
        &runtime.stored_sensor_send_interval_s,
        &runtime.setup_mode,
        &runtime.setup_ap_aktiv,
        &runtime.restart_pending,
        &runtime.restart_requested_at_ms,
        runtime.setup_ap_ssid,
        sizeof(runtime.setup_ap_ssid),
        &netErlHallProvisioningHandler,
        provisioningLog);

    if (!runtime.provisioning_bereit) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
        return;
    }

    wendeReportIntervalAn(nodeProvisioning.sanitizeStatusSendInterval(runtime.report_interval_s));
    runtime.stored_sensor_send_interval_s =
        nodeProvisioning.sanitizeSensorSendInterval(runtime.stored_sensor_send_interval_s);

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO", "config report_interval_s=%u stored_sensor_send_interval_s=%u auto_off_delay_s=%u lux_threshold=%u",
         (unsigned)runtime.report_interval_s,
         (unsigned)runtime.stored_sensor_send_interval_s,
         (unsigned)runtime.auto_off_delay_s,
         (unsigned)runtime.auto_on_lux_threshold);

    // Ohne persistierte Master-Bindung geht der Geraetepfad direkt
    // in den projektweiten Provisioning-Modus statt blind normal weiterzulaufen.
    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        nodeProvisioning.enterSetupMode();
        return;
    }

    initialisiereSensorik();
    initialisiereFunk();
    sendeHello();
}

void loop() {
    nodeProvisioning.update();

    if (!runtime.provisioning_bereit || runtime.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    if (!runtime.funk_bereit) {
        initialisiereFunk();
    }

    const unsigned long jetzt = millis();

    leseUmweltsensoren(jetzt);
    pollPIR(jetzt);
    loggeSnapshot(jetzt);
    sendeAusstehendesMotionEvent();

    if (!runtime.master_bekannt &&
        (runtime.letztes_hello_ms == 0UL ||
         (jetzt - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS)) {
        sendeHello();
    }

    if (runtime.master_bekannt &&
        (runtime.letzter_heartbeat_ms == 0UL ||
         (jetzt - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS)) {
        sendeHeartbeat();
    }

    const bool stateFaellig =
        runtime.master_bekannt &&
        runtime.master_mac_gueltig &&
        (runtime.state_report_offen ||
         runtime.letzter_state_ms == 0UL ||
         (runtime.state_interval_ms > 0UL &&
          (jetzt - runtime.letzter_state_ms) >= runtime.state_interval_ms));

    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
