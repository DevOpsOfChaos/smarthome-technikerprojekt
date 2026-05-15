// =============================================================================
// main.cpp – NET-ERL Hall Light: Flurlicht mit Praesenz- und Tageslichtsteuerung
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_hall_light/main.cpp
// Hardware:   ESP32-C3 + BME280 + VEML7700 + PIR + Relais
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN: Flur-/Eingangslicht mit automatischer Steuerung]
// === EINSATZZWECK ===
//
// Pin-Belegung (siehe PinConfig.h):
//   Relais:   GPIO10 (active-HIGH)
//   PIR:      GPIO6
//   I2C SDA:  GPIO0 (BME280: 0x76, VEML7700: 0x10)
//   I2C SCL:  GPIO1
//   Button:   GPIO2 (active-LOW)
//   Setup-LED: GPIO7
//
// Funktionsweise:
//   PIR-Motion mit Timer-Nachlauf. Auto-On bei Bewegung + Lux < Schwelle.
//   Auto-Off nach Timer (auto_off_delay_s) oder via MQTT-Befehl.
//   SET_RELAY vom Master hebelt Auto-Modus aus (relay_auto_owned=false).
//   BME280 + VEML7700 gepollt, STATE via RelayComfortConfigStateReportPayload.
//   CMD/CFG-Queue mit ISR-sicheren Pending-Requests.
//   Sensor auto-recovery alle 30s bei Fehlern.
//
// Auto-Licht Logik:
//   1. PIR erkannt + Lux noch nicht bekannt (0xFFFF): pending_auto_on_decision=true
//   2. Erster gueltiger Lux-Wert: prüft Schwelle, schaltet ggf. ein
//   3. motion_aktiv=true solange PIR HIGH oder timer laeuft
//   4. Bei motion_aktiv=false: auto_off (nur wenn relay_auto_owned)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_task_wdt.h>
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

// =============================================================================
// KONSTANTEN – Geräte-Identität, Storage, Watchdog
// =============================================================================

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
static_assert(sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "NET_ERL_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");
constexpr uint32_t NET_ERL_HALL_SETUP_MAGIC = 0x484C4C31UL;
constexpr uint16_t NET_ERL_HALL_SETUP_VERSION = 1U;
constexpr uint32_t TASK_WDT_TIMEOUT_S = 8UL;
constexpr uint16_t I2C_TIMEOUT_MS = 50U;

// =============================================================================
// GLOBALE OBJEKTE – Sensoren, Zustand, Persistenz
// =============================================================================

Adafruit_BME280 bme280;
Adafruit_VEML7700 veml = Adafruit_VEML7700();

struct SensorState {
    int16_t temp_01c;       // Temperatur in Zehntel-Grad
    uint16_t hum_01pct;     // Feuchte in Zehntel-Prozent
    uint16_t lux;           // Lux (0xFFFF=ungueltig)
    bool motion;            // Bewegung (PIR HIGH)
    bool fault;             // Sensor-Fehler
};

// PendingCmdRequest – fuer ISR-sichere ESP-NOW-Verarbeitung
struct PendingCmdRequest { bool aktiv; uint8_t sender_mac[6]; SmartHome::MsgHeader header; SmartHome::CmdPayload payload; };
struct PendingCfgRequest { bool aktiv; uint8_t sender_mac[6]; SmartHome::MsgHeader header; SmartHome::CfgPayload payload; };

// HallRuntime – Zentraler Zustand des Hall-Light-Geraets
struct HallRuntime {
    bool provisioning_bereit, setup_mode, setup_ap_aktiv, restart_pending;
    bool funk_bereit, motion_aktiv, relay_1, fault;
    bool bme_ok, lux_ok, pir_raw, relay_auto_owned, blocked_by_lux, pending_auto_on_decision;
    uint8_t pending_motion_event_state, pending_relay_event_trigger;
    unsigned long restart_requested_at_ms, letztes_hello_ms, letzter_heartbeat_ms, letzter_state_ms;
    unsigned long letztes_sensor_poll_ms, letztes_env_sample_ms, letzter_bme_recovery_ms, letzter_lux_recovery_ms;
    unsigned long letztes_snapshot_log_ms, motion_deadline_ms, state_interval_ms;
    uint8_t master_mac[6], naechste_seq;
    bool master_bekannt, master_mac_gueltig, state_report_offen;
    uint32_t report_interval_s, stored_sensor_send_interval_s;
    uint16_t auto_on_lux_threshold, auto_off_delay_s;
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE];
    SensorState sensor;
    PendingCmdRequest pending_cmd;
    PendingCfgRequest pending_cfg;
};

struct HallPersistedSetupData { uint32_t magic; uint16_t version; uint16_t reserved; uint16_t autoOnLuxThreshold; uint16_t autoOffDelayS; };
struct HallProvisioningSnapshot { uint16_t auto_on_lux_threshold; uint16_t auto_off_delay_s; };

HallRuntime runtime = {};
portMUX_TYPE runtimeMux = portMUX_INITIALIZER_UNLOCKED;

// =============================================================================
// HILFSFUNKTIONEN – Logging, Watchdog, Parser, String
// =============================================================================

bool parseUIntValue(const char* text, uint32_t& outValue) {
    if (!text || !*text) return false;
    uint32_t parsed = 0UL;
    for (const char* c = text; *c; ++c) {
        if (*c < '0' || *c > '9') return false;
        uint32_t d = (uint32_t)(*c - '0');
        if (parsed > (0xFFFFFFFFUL - d) / 10UL) return false;
        parsed = parsed * 10UL + d;
    }
    outValue = parsed; return true;
}

String htmlEscapeLocal(const String& s) {
    String e; e.reserve(s.length() + 16U);
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        switch (c) { case '&': e += "&amp;"; break; case '<': e += "&lt;"; break;
            case '>': e += "&gt;"; break; case '"': e += "&quot;"; break;
            case '\'': e += "&#39;"; break; default: e += c; break; }
    }
    return e;
}

uint16_t clampToU16(long v) { return v < 0L ? 0U : v > 65535L ? 65535U : (uint16_t)v; }

void logf(const char* l, const char* f, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;
    char m[224]; va_list a; va_start(a, f); vsnprintf(m, sizeof(m), f, a); va_end(a);
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void initialisiereTaskWatchdog() {
    esp_err_t e = esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { logf("WARN", "WDT Init err=%d", (int)e); return; }
    e = esp_task_wdt_add(nullptr);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) logf("WARN", "WDT Add err=%d", (int)e);
}

void provisioningLog(const char* l, const char* m) {
    if (!DEBUG_LOKAL_AKTIV || !l || !m) return;
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void copyText(char* t, size_t s, const char* src) {
    if (!t || !s) return; if (!src) { t[0] = '\0'; return; }
    strncpy(t, src, s - 1); t[s - 1] = '\0';
}

bool istBroadcastMac(const uint8_t* m) { return m && memcmp(m, BROADCAST_MAC, 6) == 0; }
bool senderIstMaster(const uint8_t* m) {
    return runtime.master_mac_gueltig && m && memcmp(m, runtime.master_mac, 6) == 0;
}
const uint8_t* helloZiel() { return runtime.master_mac_gueltig ? runtime.master_mac : BROADCAST_MAC; }
void wendeReportInterval(uint32_t s) { runtime.report_interval_s = s; runtime.state_interval_ms = s * 1000UL; }

// =============================================================================
// PERSISTENZ – Hall-spezifische Daten (Lux-Schwelle, Auto-Off-Delay)
// =============================================================================

void holeSnapshot(HallProvisioningSnapshot& s) { s.auto_on_lux_threshold = runtime.auto_on_lux_threshold; s.auto_off_delay_s = runtime.auto_off_delay_s; }
void wendeSnapshot(const HallProvisioningSnapshot& s) { runtime.auto_on_lux_threshold = s.auto_on_lux_threshold; runtime.auto_off_delay_s = s.auto_off_delay_s; }
bool hallDataGueltig(const HallPersistedSetupData& d) { return d.magic == NET_ERL_HALL_SETUP_MAGIC && d.version == NET_ERL_HALL_SETUP_VERSION; }
HallPersistedSetupData baueHallDaten() { HallPersistedSetupData d = {}; d.magic = NET_ERL_HALL_SETUP_MAGIC; d.version = NET_ERL_HALL_SETUP_VERSION; d.autoOnLuxThreshold = runtime.auto_on_lux_threshold; d.autoOffDelayS = runtime.auto_off_delay_s; return d; }
void wendeHallDaten(const HallPersistedSetupData& d) { runtime.auto_on_lux_threshold = d.autoOnLuxThreshold; runtime.auto_off_delay_s = d.autoOffDelayS; }

// =============================================================================
// HARDWARE – Relais
// =============================================================================

void setzeRelay(bool an, const char* grund) {
    digitalWrite(PIN_RELAY_1, (an == RELAY_1_ACTIVE_HIGH) ? HIGH : LOW);
    #if PIN_STATUS_LED >= 0
    digitalWrite(PIN_STATUS_LED, an ? HIGH : LOW);
    #endif
    runtime.relay_1 = an;
    logf("INFO", "Relay %s (%s)", an ? "ON" : "OFF", grund ? grund : "?");
}

// =============================================================================
// PROVISIONING-HANDLER (Web-Konfiguration: Lux-Schwelle + Auto-Off-Delay)
// =============================================================================

class NetErlHallProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
public:
    const char* pageTitle() const override { return "NET-ERL Hall Light"; }
    const char* pageIntro() const override { return "status_send_interval_s steuert STATE; sensor Feld wird nur mitgespeichert."; }
    const char* deviceSectionTitle() const override { return "Hall-Light"; }
    const char* deviceSectionIntro() const override { return "Nur Lux-Schwelle und Nachlauf."; }

    void loadDeviceDefaults() override { runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD; runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S; }
    bool loadDeviceSettings(Preferences& prefs) override {
        HallPersistedSetupData d = {};
        if (prefs.getBytesLength(STORAGE_KEY_HALL_SETUP) != sizeof(d)) return false;
        if (prefs.getBytes(STORAGE_KEY_HALL_SETUP, &d, sizeof(d)) != sizeof(d)) return false;
        if (!hallDataGueltig(d)) return false;
        wendeHallDaten(d); return true;
    }
    bool saveDeviceSettings(Preferences& prefs) override { auto d = baueHallDaten(); return prefs.putBytes(STORAGE_KEY_HALL_SETUP, &d, sizeof(d)) == sizeof(d); }
    bool clearDeviceSettings(Preferences& prefs) override { prefs.remove(STORAGE_KEY_HALL_SETUP); return true; }
    void captureDeviceSnapshot() override { holeSnapshot(snapshot_); }
    void restoreDeviceSnapshot() override { wendeSnapshot(snapshot_); }

    bool parseDeviceSave(WebServer& server, String& err) override {
        pending_ = {};
        uint32_t v;
        if (!parseUIntValue(server.arg("auto_on_lux_threshold").c_str(), v) || v > 65535UL) { err = "auto_on_lux_threshold ungueltig"; return false; }
        pending_.auto_on_lux_threshold = (uint16_t)v;
        if (!parseUIntValue(server.arg("auto_off_delay_s").c_str(), v) || v > 65535UL) { err = "auto_off_delay_s ungueltig"; return false; }
        pending_.auto_off_delay_s = (uint16_t)v;
        pending_.gueltig = true; return true;
    }
    void applyParsedDeviceSettings() override { if (!pending_.gueltig) return; runtime.auto_on_lux_threshold = pending_.auto_on_lux_threshold; runtime.auto_off_delay_s = pending_.auto_off_delay_s; }
    void discardParsedDeviceSettings() override { pending_ = {}; }

    void appendDeviceFieldsHtml(String& page, WebServer* src) const override {
        String lt = src && src->hasArg("auto_on_lux_threshold") ? src->arg("auto_on_lux_threshold") : String(runtime.auto_on_lux_threshold);
        String od = src && src->hasArg("auto_off_delay_s") ? src->arg("auto_off_delay_s") : String(runtime.auto_off_delay_s);
        page += "<div class=\"field\"><label>auto_on_lux_threshold</label><input name=\"auto_on_lux_threshold\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscapeLocal(lt) + "\"><div class=\"hint\">Lux-Schwelle</div></div>";
        page += "<div class=\"field\"><label>auto_off_delay_s</label><input name=\"auto_off_delay_s\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscapeLocal(od) + "\"><div class=\"hint\">Nachlauf (s)</div></div>";
    }
private:
    struct Pending { bool gueltig = false; uint16_t auto_on_lux_threshold = 0; uint16_t auto_off_delay_s = 0; };
    Pending pending_{}; HallProvisioningSnapshot snapshot_{};
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetErlHallProvisioningHandler hallProvisioningHandler;

void holeSetupSnapshot(SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, HallProvisioningSnapshot& d) { nodeProvisioning.captureBasisSnapshot(b); holeSnapshot(d); }
void wendeSetupSnapshot(const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, const HallProvisioningSnapshot& d) { nodeProvisioning.restoreBasisSnapshot(b); wendeSnapshot(d); wendeReportInterval(runtime.report_interval_s); }
bool speichereMitRollback(const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, const HallProvisioningSnapshot& d) { if (nodeProvisioning.saveCurrentState()) return true; wendeSetupSnapshot(b, d); return false; }

// =============================================================================
// ESP-NOW – Peer, Senden, ACK, Hello, Heartbeat, State, Events
// =============================================================================

bool stellePeerSicher(const uint8_t* mac) {
    if (!runtime.funk_bereit || !mac) return false;
    if (!istBroadcastMac(mac) && !SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, mac, 6); p.channel = WLAN_KANAL; p.encrypt = false;
    return esp_now_add_peer(&p) == ESP_OK;
}

bool sendePaketMitOptionen(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb, uint8_t fl, uint8_t* seq) {
    if (!runtime.funk_bereit || !z || plen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(z)) return false;
    uint8_t buf[SH_ESPNOW_MAX_BYTES] = {}; SmartHome::MsgHeader h = {};
    uint8_t s = runtime.naechste_seq++; SmartHome::fillHeader(h, mt, s, fl, plen);
    if (plen && pl) memcpy(buf + SH_HEADER_SIZE, pl, plen);
    SmartHome::finalizePacketCrc(h, buf + SH_HEADER_SIZE); memcpy(buf, &h, sizeof(h));
    if (esp_now_send(z, buf, SH_HEADER_SIZE + plen) != ESP_OK) { logf("WARN", "%s send fail", lb ? lb : "?"); return false; }
    if (seq) *seq = s; return true;
}

bool sendePaket(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb) {
    return sendePaketMitOptionen(z, mt, pl, plen, lb, 0, nullptr);
}
bool sendeAck(const uint8_t* z, uint8_t seq, uint8_t mt, uint8_t st) {
    SmartHome::AckPayload p = {}; p.ack_seq = seq; p.ack_msg_type = mt; p.status = st;
    return sendePaket(z, SH_MSG_ACK, &p, sizeof(p), "ACK");
}

uint8_t holeAutoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (runtime.lux_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    if (runtime.pending_auto_on_decision) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX;
    return f;
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN – Hello, Heartbeat, State, Events
// =============================================================================

bool sendeHello() {
    SmartHome::HelloPayload p = {};
    copyText(p.device_id, sizeof(p.device_id), DEVICE_ID);
    copyText(p.device_name, sizeof(p.device_name), DEVICE_NAME);
    p.device_class = SH_CLASS_NET_ERL; p.caps_hi = (DEVICE_CAPS >> 8) & 0xFF; p.caps_lo = DEVICE_CAPS & 0xFF;
    p.power_type = SH_POWER_MAINS; p.fw_version = 1; p.boot_counter = BOOT_COUNTER_PROTOCOL_PLACEHOLDER;
    p.meta_schema_version = DEVICE_META_SCHEMA_VERSION; p.control_mode = DEVICE_CONTROL_MODE;
    p.config_profile = DEVICE_CONFIG_PROFILE; p.reporting_mode = DEVICE_REPORTING_MODE;
    copyText(p.sensor_mask, sizeof(p.sensor_mask), "THLMXXXXXX");
    copyText(p.input_mask, sizeof(p.input_mask), "XXXXX");
    runtime.letztes_hello_ms = millis();
    return sendePaket(helloZiel(), SH_MSG_HELLO, &p, sizeof(p), "HELLO");
}

bool sendeHeartbeat() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::HeartbeatPayload p = {}; copyText(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.uptime_s = millis() / 1000UL;
    if (!sendePaket(runtime.master_mac, SH_MSG_HEARTBEAT, &p, sizeof(p), "HEARTBEAT")) return false;
    runtime.letzter_heartbeat_ms = millis(); return true;
}

bool sendeState() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::RelayComfortConfigStateReportPayload p = {};
    copyText(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.relay_1 = runtime.relay_1 ? 1U : 0U;
    p.temp_01c = runtime.sensor.temp_01c; p.hum_01pct = runtime.sensor.hum_01pct;
    p.lux = runtime.sensor.lux; p.motion = runtime.motion_aktiv ? 1U : 0U;
    p.auto_flags = holeAutoFlags(); p.fault = runtime.fault ? 1U : 0U;
    p.report_interval_s = (uint16_t)runtime.report_interval_s;
    p.auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    if (!sendePaket(runtime.master_mac, SH_MSG_STATE, &p, sizeof(p), "STATE")) return false;
    runtime.state_report_offen = false; runtime.letzter_state_ms = millis(); return true;
}

bool sendeMotionEvent(bool s) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    copyText(p.node_id, sizeof(p.node_id), DEVICE_ID); p.event_type = SH_EVENT_MOTION_DETECTED;
    p.trigger = SH_TRIGGER_AUTO; p.param1 = s ? 1U : 0U; p.param2 = 0U;
    return sendePaket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), s ? "EVENT_MOTION_ON" : "EVENT_MOTION_OFF");
}

void sendeAusstehendesMotionEvent() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig || runtime.pending_motion_event_state == 0U) return;
    if (sendeMotionEvent(runtime.pending_motion_event_state == 1U)) runtime.pending_motion_event_state = 0U;
}

bool sendeRelayEvent(uint8_t tr) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    copyText(p.node_id, sizeof(p.node_id), DEVICE_ID); p.event_type = SH_EVENT_RELAY_CHANGED;
    p.trigger = tr; p.param1 = runtime.relay_1 ? 1U : 0U; p.param2 = 0U;
    return sendePaket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), "EVENT_RELAY");
}

void merkeRelayEvent(uint8_t tr) { portENTER_CRITICAL(&runtimeMux); runtime.pending_relay_event_trigger = tr; portEXIT_CRITICAL(&runtimeMux); }
void meldeRelayAenderung(uint8_t tr) { merkeRelayEvent(tr); }

// =============================================================================
// PROTOKOLL-VERARBEITUNG – HELLO_ACK, CMD, CFG (mit ISR-sicherer Queue)
// =============================================================================

void verarbeiteHelloAck(const uint8_t* s, const SmartHome::HelloAckPayload& p) {
    if (p.ack_status != SH_ACK_OK) { logf("WARN", "HELLO_ACK abgelehnt"); return; }
    if (!runtime.master_mac_gueltig) { logf("WARN", "HELLO_ACK: keine Master-Bindung"); return; }
    if (!senderIstMaster(s)) { logf("WARN", "HELLO_ACK: falscher Sender"); return; }
    runtime.master_bekannt = true; runtime.state_report_offen = true;
    stellePeerSicher(runtime.master_mac);
}

void verarbeiteCmd(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CmdPayload& p) {
    if (!senderIstMaster(s)) { logf("WARN", "CMD ignoriert"); return; }
    if (p.cmd_type == SH_CMD_STATE_REQUEST) { runtime.state_report_offen = true; return; }
    if (p.cmd_type == SH_CMD_SET_RELAY) {
        // Prueft ob Index gueltig (nur Index 0 = relay_1)
        if (p.param1 != 0U) { if (h.flags & SH_FLAG_ACK_REQUEST) sendeAck(s, h.seq, h.msg_type, SH_ACK_REJECTED); return; }
        runtime.relay_auto_owned = false; runtime.pending_auto_on_decision = false;
        setzeRelay(p.param2 != 0U, "master_cmd");
        runtime.state_report_offen = true; meldeRelayAenderung(SH_TRIGGER_MASTER_CMD);
        if (h.flags & SH_FLAG_ACK_REQUEST) sendeAck(s, h.seq, h.msg_type, SH_ACK_OK);
        return;
    }
    if (h.flags & SH_FLAG_ACK_REQUEST) sendeAck(s, h.seq, h.msg_type, SH_ACK_REJECTED);
}

void merkePendingCmd(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CmdPayload& p) {
    PendingCmdRequest r = {}; r.aktiv = true; memcpy(r.sender_mac, s, 6); r.header = h; r.payload = p;
    portENTER_CRITICAL(&runtimeMux); runtime.pending_cmd = r; portEXIT_CRITICAL(&runtimeMux);
}

void merkePendingCfg(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CfgPayload& p) {
    PendingCfgRequest r = {}; r.aktiv = true; memcpy(r.sender_mac, s, 6); r.header = h; r.payload = p;
    portENTER_CRITICAL(&runtimeMux); runtime.pending_cfg = r; portEXIT_CRITICAL(&runtimeMux);
}

bool holePendingCmd(PendingCmdRequest& r) {
    portENTER_CRITICAL(&runtimeMux);
    if (!runtime.pending_cmd.aktiv) { portEXIT_CRITICAL(&runtimeMux); return false; }
    r = runtime.pending_cmd; runtime.pending_cmd = {}; portEXIT_CRITICAL(&runtimeMux); return true;
}

bool holePendingCfg(PendingCfgRequest& r) {
    portENTER_CRITICAL(&runtimeMux);
    if (!runtime.pending_cfg.aktiv) { portEXIT_CRITICAL(&runtimeMux); return false; }
    r = runtime.pending_cfg; runtime.pending_cfg = {}; portEXIT_CRITICAL(&runtimeMux); return true;
}

void verarbeiteAusstehende() {
    PendingCmdRequest cmd; while (holePendingCmd(cmd)) verarbeiteCmd(cmd.sender_mac, cmd.header, cmd.payload);
    PendingCfgRequest cfg; while (holePendingCfg(cfg)) verarbeiteCfg(cfg.sender_mac, cfg.header, cfg.payload);
}

void verarbeiteCfg(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CfgPayload& p);
void merkePendingCfg(const uint8_t*, const SmartHome::MsgHeader&, const SmartHome::CfgPayload&);

void verarbeiteEspNowPaket(const uint8_t* s, const uint8_t* d, int len) {
    if (!s || !d || len < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(d, (size_t)len)) return;
    const SmartHome::MsgHeader* h = reinterpret_cast<const SmartHome::MsgHeader*>(d);
    const uint8_t* pl = d + SH_HEADER_SIZE;
    switch (h->msg_type) {
        case SH_MSG_HELLO_ACK:
            if (h->payload_len == sizeof(SmartHome::HelloAckPayload))
                verarbeiteHelloAck(s, *reinterpret_cast<const SmartHome::HelloAckPayload*>(pl));
            break;
        case SH_MSG_CMD:
            if (h->payload_len == sizeof(SmartHome::CmdPayload))
                merkePendingCmd(s, *h, *reinterpret_cast<const SmartHome::CmdPayload*>(pl));
            break;
        case SH_MSG_CFG:
            if (h->payload_len == sizeof(SmartHome::CfgPayload))
                merkePendingCfg(s, *h, *reinterpret_cast<const SmartHome::CfgPayload*>(pl));
            break;
        default: break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* d, int l) { if (info) verarbeiteEspNowPaket(info->src_addr, d, l); }
#else
void onEspNowReceive(const uint8_t* s, const uint8_t* d, int l) { verarbeiteEspNowPaket(s, d, l); }
#endif
void onEspNowSend(const uint8_t*, esp_now_send_status_t s) { if (s != ESP_NOW_SEND_SUCCESS) logf("WARN", "ESP-NOW send fail"); }

// =============================================================================
// CFG-VERARBEITUNG – report_interval_s, auto_on_lux_threshold, auto_off_delay_s
// =============================================================================

bool speichereReportInterval(uint32_t v) {
    if (!nodeProvisioning.isSendIntervalValid(v)) return false;
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; HallProvisioningSnapshot ds = {};
    holeSetupSnapshot(bs, ds); wendeReportInterval(v); runtime.state_report_offen = true;
    if (!speichereMitRollback(bs, ds)) { logf("WARN", "report_interval persist fail"); return false; }
    return true;
}

bool uebernehmeCfg(const SmartHome::CfgPayload& p) {
    switch (p.param_id) {
        case SH_CFG_REPORT_INTERVAL_S:
            if (!speichereReportInterval(p.value)) return false; logf("INFO", "report_interval=%u", (unsigned)runtime.report_interval_s); return true;
        case SH_CFG_LIGHT_THRESHOLD_ON: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; HallProvisioningSnapshot ds = {};
            holeSetupSnapshot(bs, ds); runtime.auto_on_lux_threshold = (uint16_t)p.value; runtime.state_report_offen = true;
            if (!speichereMitRollback(bs, ds)) { logf("WARN", "lux_threshold persist fail"); return false; }
            logf("INFO", "lux_threshold=%u", (unsigned)runtime.auto_on_lux_threshold); return true;
        }
        case SH_CFG_AUTO_OFF_DELAY_S: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; HallProvisioningSnapshot ds = {};
            holeSetupSnapshot(bs, ds); runtime.auto_off_delay_s = (uint16_t)p.value; runtime.state_report_offen = true;
            if (!speichereMitRollback(bs, ds)) { logf("WARN", "auto_off persist fail"); return false; }
            logf("INFO", "auto_off=%u", (unsigned)runtime.auto_off_delay_s); return true;
        }
        default: return false;
    }
}

void verarbeiteCfg(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CfgPayload& p) {
    if (!senderIstMaster(s)) { logf("WARN", "CFG ignoriert"); return; }
    bool ok = uebernehmeCfg(p);
    if (h.flags & SH_FLAG_ACK_REQUEST) sendeAck(s, h.seq, h.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
}

// =============================================================================
// FUNK-INIT + SENSOR-INIT
// =============================================================================

void initialisiereFunk() {
    if (runtime.funk_bereit || runtime.setup_mode) return;
    WiFi.mode(WIFI_STA); WiFi.disconnect(); WiFi.setSleep(false);
    esp_wifi_set_channel(WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) { logf("WARN", "ESP-NOW init fail"); return; }
    esp_now_register_send_cb(onEspNowSend); esp_now_register_recv_cb(onEspNowReceive);
    runtime.funk_bereit = true; stellePeerSicher(BROADCAST_MAC);
    if (runtime.master_mac_gueltig) stellePeerSicher(runtime.master_mac);
}

void konfiguriereVeml7700() { veml.setGain(VEML7700_GAIN_1); veml.setIntegrationTime(VEML7700_IT_100MS); }

void initialisiereSensorik() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL); Wire.setTimeOut(I2C_TIMEOUT_MS);
    runtime.bme_ok = bme280.begin(NET_ERL_BME280_ADDRESS, &Wire);
    if (!runtime.bme_ok) logf("WARN", "BME280 nicht gefunden (0x%02X)", NET_ERL_BME280_ADDRESS);
    if (!veml.begin()) { runtime.lux_ok = false; logf("WARN", "VEML7700 nicht gefunden"); }
    else { runtime.lux_ok = true; konfiguriereVeml7700(); }
    pinMode(PIN_PIR, INPUT);
}

void setzeSensorDefaults() {
    runtime.sensor.temp_01c = INT16_MIN; runtime.sensor.hum_01pct = 0xFFFFU;
    runtime.sensor.lux = 0xFFFFU; runtime.sensor.motion = false; runtime.sensor.fault = false;
}

bool sensorRecoveryFaellig(unsigned long l, unsigned long j) { return l == 0UL || (j - l) >= SENSOR_RECOVERY_RETRY_INTERVAL_MS; }

void versucheBmeRecovery(unsigned long j) {
    if (runtime.bme_ok || !sensorRecoveryFaellig(runtime.letzter_bme_recovery_ms, j)) return;
    runtime.letzter_bme_recovery_ms = j;
    runtime.bme_ok = bme280.begin(NET_ERL_BME280_ADDRESS, &Wire);
    logf(runtime.bme_ok ? "INFO" : "WARN", "BME280 recovery %s", runtime.bme_ok ? "ok" : "fail");
}

void versucheLuxRecovery(unsigned long j) {
    if (runtime.lux_ok || !sensorRecoveryFaellig(runtime.letzter_lux_recovery_ms, j)) return;
    runtime.letzter_lux_recovery_ms = j;
    runtime.lux_ok = veml.begin();
    if (runtime.lux_ok) konfiguriereVeml7700();
    logf(runtime.lux_ok ? "INFO" : "WARN", "VEML7700 recovery %s", runtime.lux_ok ? "ok" : "fail");
}

void leseUmweltsensoren(unsigned long j) {
    if ((j - runtime.letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    runtime.letztes_env_sample_ms = j;
    versucheBmeRecovery(j); versucheLuxRecovery(j);

    if (runtime.bme_ok) {
        float t = bme280.readTemperature(); float h = bme280.readHumidity();
        if (!isfinite(t) || !isfinite(h)) { runtime.bme_ok = false; logf("WARN", "BME280 read fail"); }
        else { runtime.sensor.temp_01c = (int16_t)lroundf(t * 10.0f); runtime.sensor.hum_01pct = clampToU16((long)lroundf(h * 10.0f)); }
    }

    if (runtime.lux_ok) {
        float l = veml.readLux();
        if (!isnan(l) && l >= 0.0f) runtime.sensor.lux = (uint16_t)lroundf(l);
        else { runtime.lux_ok = false; logf("WARN", "VEML7700 read fail"); }
    }

    runtime.fault = !(runtime.bme_ok && runtime.lux_ok);
    runtime.sensor.fault = runtime.fault;

    // Auto-On Entscheidung wenn motion aktiv, aber Lux fehlte bisher
    if (runtime.motion_aktiv && runtime.pending_auto_on_decision && !runtime.relay_1 &&
        runtime.sensor.lux != 0xFFFFU) {
        runtime.pending_auto_on_decision = false;
        if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true; runtime.blocked_by_lux = false;
            setzeRelay(true, "auto_on_motion_late_lux");
            meldeRelayAenderung(SH_TRIGGER_AUTO); runtime.state_report_offen = true;
        } else { runtime.blocked_by_lux = true; runtime.state_report_offen = true; }
    }
}

// =============================================================================
// PIR + MOTION-LOGIK – Bewegung erkennen, Nachlauf, Ereignis
// =============================================================================

void motionAktiv(unsigned long j) {
    runtime.motion_aktiv = true; runtime.sensor.motion = true;
    runtime.letztes_sensor_poll_ms = j; runtime.state_report_offen = true;
    runtime.pending_motion_event_state = 1U;
    runtime.blocked_by_lux = false; runtime.pending_auto_on_decision = false;

    if (!runtime.relay_1) {
        if (runtime.sensor.lux == 0xFFFFU) {
            runtime.pending_auto_on_decision = true;
        } else if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true;
            setzeRelay(true, "auto_on_motion");
            meldeRelayAenderung(SH_TRIGGER_AUTO);
        } else {
            runtime.blocked_by_lux = true;
        }
    }
}

void pollPresence(unsigned long j) {
    if ((j - runtime.letztes_sensor_poll_ms) < NET_ERL_SENSOR_POLL_INTERVAL_MS) return;
    runtime.letztes_sensor_poll_ms = j;

    bool pir = digitalRead(PIN_PIR) == HIGH;
    runtime.pir_raw = pir;

    if (pir) {
        if (!runtime.motion_aktiv) motionAktiv(j);
        else { runtime.letztes_sensor_poll_ms = j; }
        return;
    }

    unsigned long offMs = (unsigned long)runtime.auto_off_delay_s * 1000UL;
    if (runtime.motion_aktiv && runtime.letztes_sensor_poll_ms > 0UL &&
        (j - runtime.letztes_sensor_poll_ms) >= offMs) {
        runtime.motion_aktiv = false; runtime.sensor.motion = false;
        runtime.blocked_by_lux = false; runtime.pending_auto_on_decision = false;
        runtime.state_report_offen = true;
        runtime.pending_motion_event_state = 2U;
        if (runtime.relay_1 && runtime.relay_auto_owned) {
            setzeRelay(false, "auto_off_timer"); meldeRelayAenderung(SH_TRIGGER_AUTO_OFF_TIMER);
            runtime.relay_auto_owned = false;
        }
    }
}

// =============================================================================
// ARDUINO – setup() und loop()
// =============================================================================

void setup() {
    if (DEBUG_LOKAL_AKTIV) { Serial.begin(115200); delay(150); }
    initialisiereTaskWatchdog();

    runtime = {};
    runtime.report_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.stored_sensor_send_interval_s = DEFAULT_STORED_SENSOR_SEND_INTERVAL_S;
    runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
    runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    runtime.state_report_offen = true;

    setzeSensorDefaults();
    pinMode(PIN_RELAY_1, OUTPUT); setzeRelay(false, "boot");
#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT); digitalWrite(PIN_STATUS_LED, LOW);
#endif

    auto cfg = SmartHome::NetErlProvisioning::makeConfig(DEVICE_ID,
        NET_ERL_HALL_STORAGE_NAMESPACE, NET_ERL_DEFAULT_REPORT_INTERVAL_S,
        DEFAULT_STORED_SENSOR_SEND_INTERVAL_S, MIN_REPORT_INTERVAL_S, MAX_REPORT_INTERVAL_S);
    cfg.setupButtonPin = SETUP_BUTTON_PIN; cfg.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    cfg.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS; cfg.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    cfg.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0; cfg.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    runtime.provisioning_bereit = nodeProvisioning.begin(cfg,
        &runtime.master_mac_gueltig, runtime.master_mac,
        &runtime.report_interval_s, &runtime.stored_sensor_send_interval_s,
        &runtime.setup_mode, &runtime.setup_ap_aktiv, &runtime.restart_pending,
        &runtime.restart_requested_at_ms, runtime.setup_ap_ssid, sizeof(runtime.setup_ap_ssid),
        &hallProvisioningHandler, provisioningLog);
    if (!runtime.provisioning_bereit) { logf("WARN", "Provisioning init fail"); return; }

    wendeReportInterval(nodeProvisioning.sanitizeStatusSendInterval(runtime.report_interval_s));
    runtime.stored_sensor_send_interval_s = nodeProvisioning.sanitizeSensorSendInterval(runtime.stored_sensor_send_interval_s);

    logf("INFO", "%s v%s (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "%s %s %s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);

    if (!nodeProvisioning.hasStoredMasterMac()) { nodeProvisioning.enterSetupMode(); return; }
    initialisiereSensorik(); initialisiereFunk(); sendeHello();
}

void loop() {
    esp_task_wdt_reset(); nodeProvisioning.update();
    if (!runtime.provisioning_bereit || runtime.setup_mode) { delay(LOOP_INTERVAL_MS); return; }
    if (!runtime.funk_bereit) initialisiereFunk();

    unsigned long j = millis();
    verarbeiteAusstehende();
    pollPresence(j);
    leseUmweltsensoren(j);
    sendeAusstehendesMotionEvent();

    if (!runtime.master_bekannt && (j - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) sendeHello();
    if (runtime.master_bekannt && (j - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) sendeHeartbeat();

    bool stateF = runtime.master_bekannt && runtime.master_mac_gueltig &&
        (runtime.state_report_offen || runtime.letzter_state_ms == 0UL ||
         (runtime.state_interval_ms > 0 && (j - runtime.letzter_state_ms) >= runtime.state_interval_ms));
    if (stateF) sendeState();

    if ((j - runtime.letztes_snapshot_log_ms) >= NET_ERL_SNAPSHOT_LOG_INTERVAL_MS && DEBUG_LOKAL_AKTIV) {
        logf("INFO", "snap t=%d h=%u l=%u m=%s r=%s auto=%s bl=%s fa=%s",
            (int)runtime.sensor.temp_01c, runtime.sensor.hum_01pct, runtime.sensor.lux,
            runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0",
            runtime.relay_auto_owned ? "1" : "0", runtime.blocked_by_lux ? "1" : "0",
            runtime.fault ? "1" : "0");
        runtime.letztes_snapshot_log_ms = j;
    }
    delay(LOOP_INTERVAL_MS);
}
