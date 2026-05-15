// =============================================================================
// NetErlRuntime.h – Gemeinsame Runtime für alle net_erl-Geräte (Baukasten Block 3)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_erl/NetErlRuntime.h
// Teil des:   Baukastensystems Block 3 – Geräteklassen-Runtime
//
// ENTHÄLT:
//   - setup() und loop() für ALLE net_erl-Geräte
//   - ESP-NOW-Protokoll (HELLO, HEARTBEAT, STATE, CMD, CFG, ACK)
//   - Provisioning-Integration (Web-UI für Master-MAC + Intervalle)
//   - Auto-Light-Logik (Präsenz + Lux-Schwelle + Nachlauf)
//   - Task-Watchdog
//   - Sensor-Recovery via SensorUtils.h
//
// HOOKS (muss jedes Device implementieren):
//   void    netErlDeviceInit()                  – Sensor-I2C + GPIO-Init
//   void    netErlDeviceResetSensorDefaults()   – Sensorfelder auf UNGUELTIG
//   void    netErlDevicePollSensors(unsigned long nowMs) – Alle Sensoren lesen
//   void    netErlDeviceSetRelayOutput(bool on) – Relais physisch setzen + LEDs
//   bool    netErlDeviceReadPresence()          – Bewegungssensor digital lesen
//   void    netErlDeviceFillStatePayload(void* payload, size_t* size) – State-Payload füllen
//   uint8_t netErlDeviceBuildAutoFlags()        – Auto-Light-Flags bauen
//   bool    netErlDeviceHasSensorFault()        – true wenn Sensor(en) defekt
//   void    netErlDeviceLogSnapshot()           – Debug-Log der Sensorwerte
//
// OPTIONALE HOOKS:
//   #define NET_ERL_HAS_BUTTON  → bool netErlDeviceReadButton()
//   #define NET_ERL_HAS_INDICATOR_UPDATE → void netErlDeviceUpdateIndicators(bool relayOn)
//
// KONFIGURATION (in DeviceConfig.h vor dem Include setzbar):
//   NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION  (1=Kitchen-Stil, 0=Hall-Stil)
//   NET_ERL_USE_ISR_CMD_QUEUE           (0=direkt, 1=ISR-safe Queue)
//   NET_ERL_WDT_TIMEOUT_S               (Watchdog-Timeout, Default 15)
//   NET_ERL_SENSOR_MASK                 (Hello-Sensor-Maske, z.B. "THLPGAMXXX")
//   NET_ERL_INPUT_MASK                  (Hello-Input-Maske, z.B. "BXXXX")
//   NET_ERL_DEVICE_PAGE_TITLE           (Provisioning-Seitentitel)
//   NET_ERL_DEVICE_PAGE_INTRO           (Provisioning-Einleitung)
//   NET_ERL_DEVICE_SECTION_TITLE        (Provisioning-Abschnittstitel)
//   NET_ERL_PERSISTED_MAGIC             (Magic-Number für NVS-Persistenz)
//   NET_ERL_PERSISTED_KEY               (NVS-Key)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-15
// =============================================================================

#pragma once

// =============================================================================
// PRÜFUNG: DeviceConfig.h muss vorher includiert sein
// =============================================================================
#ifndef NET_ERL_DEVICE_ID
#error "DeviceConfig.h muss VOR NetErlRuntime.h includiert werden"
#endif

// =============================================================================
// KONFIGURIERBARE DEFAULTS (per DeviceConfig.h überschreibbar)
// =============================================================================
#ifndef NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION
#define NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION 1   // 1=verlängert Nachlauf bei erneuter Bewegung (Kitchen), 0=nicht (Hall)
#endif
#ifndef NET_ERL_USE_ISR_CMD_QUEUE
#define NET_ERL_USE_ISR_CMD_QUEUE 0             // 0=direkte Verarbeitung, 1=ISR-safe Queue (Hall)
#endif
#ifndef NET_ERL_WDT_TIMEOUT_S
#define NET_ERL_WDT_TIMEOUT_S 15UL
#endif
#ifndef NET_ERL_SENSOR_MASK
#define NET_ERL_SENSOR_MASK "THLXXXXXXX"
#endif
#ifndef NET_ERL_INPUT_MASK
#define NET_ERL_INPUT_MASK "BXXXX"
#endif
#ifndef NET_ERL_DEVICE_PAGE_TITLE
#define NET_ERL_DEVICE_PAGE_TITLE "NET-ERL Device"
#endif
#ifndef NET_ERL_DEVICE_PAGE_INTRO
#define NET_ERL_DEVICE_PAGE_INTRO "status_send_interval_s steuert STATE"
#endif
#ifndef NET_ERL_DEVICE_SECTION_TITLE
#define NET_ERL_DEVICE_SECTION_TITLE "Device"
#endif
#ifndef NET_ERL_DEVICE_SECTION_INTRO
#define NET_ERL_DEVICE_SECTION_INTRO "Geraetespezifische Einstellungen."
#endif
#ifndef NET_ERL_PERSISTED_MAGIC
#define NET_ERL_PERSISTED_MAGIC 0x45524C30UL
#endif
#ifndef NET_ERL_PERSISTED_KEY
#define NET_ERL_PERSISTED_KEY "net_erl_cfg_v1"
#endif

// =============================================================================
// INCLUDES
// =============================================================================
#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdarg.h>

#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../include/MathUtils.h"
#include "../../../include/SensorUtils.h"
#include "../../../include/TimingUtils.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"
#include "NetErlProvisioning.h"

using SmartHome::clampToU16;
using SmartHome::clampHum01pct;
using SmartHome::absDiffU16;
using SmartHome::recoveryIsDue;
using SmartHome::gasWarmupComplete;
using SmartHome::sensorValueStale;
using SmartHome::intervalElapsed;

// =============================================================================
// KONSTANTEN
// =============================================================================
constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-ERL";
constexpr char DATEI_VERSION[] = "0.6.0";  // Runtime-Version
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

constexpr char DEVICE_ID[] = NET_ERL_DEVICE_ID;
constexpr char DEVICE_NAME[] = NET_ERL_DEVICE_NAME;
constexpr char FW_VARIANT[] = NET_ERL_FW_VARIANT;
constexpr uint16_t DEVICE_CAPS = NET_ERL_DEVICE_CAPS;
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
constexpr uint32_t BOOT_COUNTER = NET_ERL_BOOT_COUNTER;
constexpr uint32_t SNAPSHOT_LOG_INTERVAL_MS = NET_ERL_SNAPSHOT_LOG_INTERVAL_MS;

constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
constexpr const char* STORAGE_NS = NET_ERL_STORAGE_NS;
constexpr uint16_t STORAGE_VERSION = 1U;
constexpr uint16_t PERSISTED_MAGIC = NET_ERL_PERSISTED_MAGIC;

static_assert(sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE, "DEVICE_ID passt nicht in SSID-Puffer");

// =============================================================================
// RUNTIME-STRUKTUR (gemeinsamer Zustand aller net_erl-Geräte)
// =============================================================================
struct NetErlRuntime {
    // Provisioning & Funk
    bool provisioning_bereit, setup_mode, setup_ap_aktiv, restart_pending, funk_bereit;
    bool master_bekannt, master_mac_gueltig;
    uint8_t master_mac[6];
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE];

    // Relais & Auto-Light
    bool relay_1, relay_auto_owned, blocked_by_lux, pending_auto_on_decision;
    uint16_t auto_on_lux_threshold, auto_off_delay_s;
    unsigned long letzte_motion_ms;

    // Sensor-Status
    bool fault;

    // Timer
    unsigned long boot_ms, state_interval_ms, report_interval_s, stored_sensor_send_interval_s;
    unsigned long letztes_hello_ms, letzter_heartbeat_ms, letzter_state_ms;
    unsigned long letztes_sensor_poll_ms, letztes_snap_log_ms;
    unsigned long restart_requested_at_ms;

    // ESP-NOW
    uint8_t naechste_seq;
    bool state_report_offen;

    // Präsenz (Bewegung)
    bool motion_aktiv;
    uint8_t pending_motion_event_state;

    // Sensor-Recovery (für alle I2C-Sensoren)
    unsigned long letzter_recovery_ms[4];  // Index 0=BME, 1=LUX, 2=ENS, 3=reserve
    bool sensor_ok[4];                     // Index parallel zu letzter_recovery_ms

#if NET_ERL_USE_ISR_CMD_QUEUE
    // ISR-safe CMD/CFG-Pending-Queue (Hall-Stil)
    portMUX_TYPE runtimeMux = portMUX_INITIALIZER_UNLOCKED;
    bool pendingCmdReady, pendingCfgReady;
    SmartHome::CmdPayload pendingCmd;
    SmartHome::MsgHeader pendingCmdHeader;
    uint8_t pendingCmdSrc[6];
    SmartHome::CfgPayload pendingCfg;
    SmartHome::MsgHeader pendingCfgHeader;
    uint8_t pendingCfgSrc[6];
#endif

    // Button (optional)
#if defined(NET_ERL_HAS_BUTTON)
    bool button_raw_active, button_stable_active;
    unsigned long button_changed_at_ms, button_pressed_at_ms;
#endif
};

// =============================================================================
// PERSISTENZ-STRUKTUREN
// =============================================================================
struct NetErlPersistedData {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint16_t autoOnLuxThreshold;
    uint16_t autoOffDelayS;
};

struct NetErlSnapshot {
    uint16_t auto_on_lux_threshold;
    uint16_t auto_off_delay_s;
};

// =============================================================================
// GLOBALE OBJEKTE
// =============================================================================
NetErlRuntime runtime = {};

// =============================================================================
// HOOK-DEKLARATIONEN (müssen vom Device implementiert werden)
// =============================================================================
#ifndef NET_ERL_DEVICE_HAS_CUSTOM_HOOKS
// No-Op-Defaults für Basistyp-Builds (ohne Device)
inline void netErlDeviceInit() {}
inline void netErlDeviceResetSensorDefaults() {}
inline void netErlDevicePollSensors(unsigned long) {}
inline void netErlDeviceSetRelayOutput(bool) {}
inline bool netErlDeviceReadPresence() { return false; }
inline void netErlDeviceFillStatePayload(void*, size_t*) {}
inline uint8_t netErlDeviceBuildAutoFlags() { return 0; }
inline bool netErlDeviceHasSensorFault() { return false; }
inline void netErlDeviceLogSnapshot() {}
#else
// Deklariert – Device-main.cpp liefert die Implementierung
extern void netErlDeviceInit();
extern void netErlDeviceResetSensorDefaults();
extern void netErlDevicePollSensors(unsigned long nowMs);
extern void netErlDeviceSetRelayOutput(bool on);
extern bool netErlDeviceReadPresence();
extern void netErlDeviceFillStatePayload(void* payload, size_t* payloadSize);
extern uint8_t netErlDeviceBuildAutoFlags();
extern bool netErlDeviceHasSensorFault();
extern void netErlDeviceLogSnapshot();

#ifdef NET_ERL_HAS_INDICATOR_UPDATE
extern void netErlDeviceUpdateIndicators(bool relayOn);
#else
inline void netErlDeviceUpdateIndicators(bool) {}
#endif

#ifdef NET_ERL_HAS_BUTTON
extern bool netErlDeviceReadButton();
#endif
#endif

// =============================================================================
// HILFSFUNKTIONEN
// =============================================================================

bool parseUInt(const char* t, uint32_t& v) {
    if (!t || !*t) return false; v = 0;
    for (const char* c = t; *c; ++c) {
        if (*c < '0' || *c > '9') return false;
        uint32_t d = *c - '0';
        if (v > (0xFFFFFFFFUL - d) / 10UL) return false;
        v = v * 10 + d;
    }
    return true;
}

String htmlEscape(const String& s) {
    String e; e.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); ++i) { char c = s[i];
        switch (c) { case '&': e += "&amp;"; break; case '<': e += "&lt;"; break; case '>': e += "&gt;"; break; case '"': e += "&quot;"; break; case '\'': e += "&#39;"; break; default: e += c; }
    } return e;
}

void logf(const char* l, const char* f, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;
    char m[224]; va_list a; va_start(a, f); vsnprintf(m, sizeof(m), f, a); va_end(a);
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void initWdt() {
    esp_err_t e = esp_task_wdt_init(NET_ERL_WDT_TIMEOUT_S, true);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { logf("WARN", "WDT init err=%d", (int)e); return; }
    e = esp_task_wdt_add(nullptr);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) logf("WARN", "WDT add err=%d", (int)e);
}

void provLog(const char* l, const char* m) {
    if (!DEBUG_LOKAL_AKTIV || !l || !m) return;
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void cpy(char* t, size_t s, const char* src) {
    if (!t || !s) return; if (!src) { t[0] = '\0'; return; }
    strncpy(t, src, s - 1); t[s - 1] = '\0';
}

bool isBroadcast(const uint8_t* m) { return m && memcmp(m, BROADCAST_MAC, 6) == 0; }
bool isMaster(const uint8_t* m) { return runtime.master_mac_gueltig && m && memcmp(m, runtime.master_mac, 6) == 0; }
const uint8_t* helloDst() { return runtime.master_mac_gueltig ? runtime.master_mac : BROADCAST_MAC; }
void setReportInt(uint32_t s) { runtime.report_interval_s = s; runtime.state_interval_ms = s * 1000UL; }

// =============================================================================
// RELAIS – physische Ansteuerung delegiert an Device-Hook
// =============================================================================
void setRelay(bool an, const char* g) {
    netErlDeviceSetRelayOutput(an);
    runtime.relay_1 = an;
    netErlDeviceUpdateIndicators(an);
    logf("INFO", "Relay %s (%s)", an ? "ON" : "OFF", g ? g : "?");
}

// =============================================================================
// PERSISTENZ-HILFEN
// =============================================================================
void snapK(NetErlSnapshot& s) {
    s.auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    s.auto_off_delay_s = runtime.auto_off_delay_s;
}
void restK(const NetErlSnapshot& s) {
    runtime.auto_on_lux_threshold = s.auto_on_lux_threshold;
    runtime.auto_off_delay_s = s.auto_off_delay_s;
}
void snapBasis(SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, NetErlSnapshot& d) {
    nodeProvisioning.captureBasisSnapshot(b); snapK(d);
}
void restBasis(const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, const NetErlSnapshot& d) {
    nodeProvisioning.restoreBasisSnapshot(b); restK(d);
    setReportInt(runtime.report_interval_s);
}

// =============================================================================
// PROVISIONING-HANDLER (gemeinsam, per #define parametrisiert)
// =============================================================================

class NetErlProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
public:
    const char* pageTitle() const override { return NET_ERL_DEVICE_PAGE_TITLE; }
    const char* pageIntro() const override { return NET_ERL_DEVICE_PAGE_INTRO; }
    const char* deviceSectionTitle() const override { return NET_ERL_DEVICE_SECTION_TITLE; }
    const char* deviceSectionIntro() const override { return NET_ERL_DEVICE_SECTION_INTRO; }

    void loadDeviceDefaults() override {
        runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
        runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    }

    bool loadDeviceSettings(Preferences& p) override {
        NetErlPersistedData d = {};
        if (p.getBytesLength(NET_ERL_PERSISTED_KEY) != sizeof(d)) return false;
        if (p.getBytes(NET_ERL_PERSISTED_KEY, &d, sizeof(d)) != sizeof(d)) return false;
        if (d.magic != PERSISTED_MAGIC || d.version != STORAGE_VERSION) return false;
        runtime.auto_on_lux_threshold = d.autoOnLuxThreshold;
        runtime.auto_off_delay_s = d.autoOffDelayS;
        return true;
    }

    bool saveDeviceSettings(Preferences& p) override {
        NetErlPersistedData d = {};
        d.magic = PERSISTED_MAGIC; d.version = STORAGE_VERSION;
        d.autoOnLuxThreshold = runtime.auto_on_lux_threshold;
        d.autoOffDelayS = runtime.auto_off_delay_s;
        return p.putBytes(NET_ERL_PERSISTED_KEY, &d, sizeof(d)) == sizeof(d);
    }

    bool clearDeviceSettings(Preferences& p) override { p.remove(NET_ERL_PERSISTED_KEY); return true; }
    void captureDeviceSnapshot() override { snapK(s_); }
    void restoreDeviceSnapshot() override { restK(s_); }

    bool parseDeviceSave(WebServer& srv, String& err) override {
        p_ = {}; uint32_t v;
        if (!parseUInt(srv.arg("auto_on_lux_threshold").c_str(), v) || v > 65535UL) { err = "auto_on_lux_threshold ungueltig"; return false; }
        p_.auto_on_lux_threshold = (uint16_t)v;
        if (!parseUInt(srv.arg("auto_off_delay_s").c_str(), v) || v > 65535UL) { err = "auto_off_delay_s ungueltig"; return false; }
        p_.auto_off_delay_s = (uint16_t)v; p_.g = true; return true;
    }

    void applyParsedDeviceSettings() override {
        if (!p_.g) return;
        runtime.auto_on_lux_threshold = p_.auto_on_lux_threshold;
        runtime.auto_off_delay_s = p_.auto_off_delay_s;
    }

    void discardParsedDeviceSettings() override { p_ = {}; }

    void appendDeviceFieldsHtml(String& page, WebServer* src) const override {
        String lt = src && src->hasArg("auto_on_lux_threshold") ? src->arg("auto_on_lux_threshold") : String(runtime.auto_on_lux_threshold);
        String od = src && src->hasArg("auto_off_delay_s") ? src->arg("auto_off_delay_s") : String(runtime.auto_off_delay_s);
        page += "<div class=\"field\"><label>auto_on_lux_threshold</label><input name=\"auto_on_lux_threshold\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscape(lt) + "\"><div class=\"hint\">Lux-Schwelle</div></div>";
        page += "<div class=\"field\"><label>auto_off_delay_s</label><input name=\"auto_off_delay_s\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscape(od) + "\"><div class=\"hint\">Nachlauf (s)</div></div>";
    }

private:
    struct P { bool g = false; uint16_t auto_on_lux_threshold = 0; uint16_t auto_off_delay_s = 0; } p_{};
    NetErlSnapshot s_{};
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetErlProvisioningHandler provisioningHandler;

// =============================================================================
// ESP-NOW INFRASTRUKTUR
// =============================================================================

bool ensurePeer(const uint8_t* m) {
    if (!runtime.funk_bereit || !m) return false;
    if (!isBroadcast(m) && !SmartHome::isValidMac(m)) return false;
    if (esp_now_is_peer_exist(m)) return true;
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, m, 6);
    p.channel = WLAN_KANAL; p.encrypt = false;
    return esp_now_add_peer(&p) == ESP_OK;
}

bool sendPacketOpt(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb, uint8_t fl, uint8_t* seq) {
    if (!runtime.funk_bereit || !z || plen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!ensurePeer(z)) return false;
    uint8_t buf[SH_ESPNOW_MAX_BYTES] = {};
    SmartHome::MsgHeader h = {};
    uint8_t s = runtime.naechste_seq++;
    SmartHome::fillHeader(h, mt, s, fl, plen);
    if (plen && pl) memcpy(buf + SH_HEADER_SIZE, pl, plen);
    SmartHome::finalizePacketCrc(h, buf + SH_HEADER_SIZE);
    memcpy(buf, &h, sizeof(h));
    if (esp_now_send(z, buf, SH_HEADER_SIZE + plen) != ESP_OK) {
        logf("WARN", "%s send fail", lb ? lb : "?"); return false;
    }
    if (seq) *seq = s; return true;
}

bool sendPacket(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb) {
    return sendPacketOpt(z, mt, pl, plen, lb, 0, nullptr);
}

bool sendAck(const uint8_t* z, uint8_t s, uint8_t mt, uint8_t st) {
    SmartHome::AckPayload p = {}; p.ack_seq = s; p.ack_msg_type = mt; p.status = st;
    return sendPacket(z, SH_MSG_ACK, &p, sizeof(p), "ACK");
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN
// =============================================================================

bool sendHello() {
    SmartHome::HelloPayload p = {};
    cpy(p.device_id, sizeof(p.device_id), DEVICE_ID);
    cpy(p.device_name, sizeof(p.device_name), DEVICE_NAME);
    p.device_class = SH_CLASS_NET_ERL;
    p.caps_hi = (DEVICE_CAPS >> 8) & 0xFF; p.caps_lo = DEVICE_CAPS & 0xFF;
    p.power_type = SH_POWER_MAINS; p.fw_version = 1; p.boot_counter = BOOT_COUNTER;
    p.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    p.control_mode = DEVICE_CONTROL_MODE;
    p.config_profile = DEVICE_CONFIG_PROFILE; p.reporting_mode = DEVICE_REPORTING_MODE;
    cpy(p.sensor_mask, sizeof(p.sensor_mask), NET_ERL_SENSOR_MASK);
    cpy(p.input_mask, sizeof(p.input_mask), NET_ERL_INPUT_MASK);
    runtime.letztes_hello_ms = millis();
    return sendPacket(helloDst(), SH_MSG_HELLO, &p, sizeof(p), "HELLO");
}

bool sendHeartbeat() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::HeartbeatPayload p = {};
    cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.uptime_s = millis() / 1000UL;
    if (!sendPacket(runtime.master_mac, SH_MSG_HEARTBEAT, &p, sizeof(p), "HEARTBEAT")) return false;
    runtime.letzter_heartbeat_ms = millis(); return true;
}

bool sendState() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    // Payload-Größe und Inhalt werden vom Device-Hook bestimmt
    size_t ps = 0UL;
    netErlDeviceFillStatePayload(nullptr, &ps);  // Erster Aufruf: nur Größe erfragen
    if (ps == 0UL || ps > SH_MAX_PAYLOAD_BYTES) return false;
    uint8_t buf[SH_MAX_PAYLOAD_BYTES] = {};
    netErlDeviceFillStatePayload(buf, &ps);      // Zweiter Aufruf: Daten schreiben
    if (!sendPacket(runtime.master_mac, SH_MSG_STATE, buf, ps, "STATE")) return false;
    runtime.state_report_offen = false; runtime.letzter_state_ms = millis(); return true;
}

bool sendMotionEvent(bool s) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_MOTION_DETECTED; p.trigger = SH_TRIGGER_AUTO;
    p.param1 = s ? 1U : 0U; p.param2 = 0U;
    return sendPacket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), s ? "EVT_M_ON" : "EVT_M_OFF");
}

bool sendRelayEvent(uint8_t tr) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_RELAY_CHANGED; p.trigger = tr;
    p.param1 = runtime.relay_1 ? 1U : 0U; p.param2 = 0U;
    return sendPacket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), "EVT_RELAY");
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG
// =============================================================================

void handleHelloAck(const uint8_t* s, const SmartHome::HelloAckPayload& p) {
    if (p.ack_status != SH_ACK_OK) { logf("WARN", "HELLO_ACK status=%d", (int)p.ack_status); return; }
    if (!runtime.master_mac_gueltig) { logf("WARN", "HELLO_ACK: master MAC nicht gueltig"); return; }
    if (!isMaster(s)) { logf("WARN", "HELLO_ACK: Absender ist nicht Master"); return; }
    runtime.master_bekannt = true; runtime.state_report_offen = true;
    ensurePeer(runtime.master_mac);
}

void handleCmd(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CmdPayload& p) {
    if (!isMaster(s)) return;
    if (p.cmd_type == SH_CMD_STATE_REQUEST) { runtime.state_report_offen = true; return; }
    if (p.cmd_type == SH_CMD_SET_RELAY) {
        if (p.param1 != 0U) {  // Subcommand ungleich 0 → nicht unterstützt
            if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_REJECTED);
            return;
        }
        runtime.relay_auto_owned = false; runtime.pending_auto_on_decision = false;
        runtime.blocked_by_lux = false;
        setRelay(p.param2 != 0U, "master");
        runtime.state_report_offen = true;
        sendRelayEvent(SH_TRIGGER_MASTER_CMD);
        if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_OK);
        return;
    }
    if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_REJECTED);
}

bool saveReportIntCfg(uint32_t v) {
    if (!nodeProvisioning.isSendIntervalValid(v)) return false;
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; NetErlSnapshot ds = {};
    snapBasis(bs, ds); setReportInt(v); runtime.state_report_offen = true;
    if (nodeProvisioning.saveCurrentState()) return true;
    restBasis(bs, ds); return false;
}

bool handleCfg(const SmartHome::CfgPayload& p) {
    switch (p.param_id) {
        case SH_CFG_REPORT_INTERVAL_S: return saveReportIntCfg(p.value);
        case SH_CFG_LIGHT_THRESHOLD_ON: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; NetErlSnapshot ds = {};
            snapBasis(bs, ds); runtime.auto_on_lux_threshold = (uint16_t)p.value; runtime.state_report_offen = true;
            if (nodeProvisioning.saveCurrentState()) return true; restBasis(bs, ds); return false;
        }
        case SH_CFG_AUTO_OFF_DELAY_S: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; NetErlSnapshot ds = {};
            snapBasis(bs, ds); runtime.auto_off_delay_s = (uint16_t)p.value; runtime.state_report_offen = true;
            if (nodeProvisioning.saveCurrentState()) return true; restBasis(bs, ds); return false;
        }
        default: return false;
    }
}

#if NET_ERL_USE_ISR_CMD_QUEUE
// ISR-safe Pending-Queue (Hall-Stil)
void merkePendingCmd(const uint8_t* src, const SmartHome::MsgHeader& h, const SmartHome::CmdPayload& p) {
    portENTER_CRITICAL(&runtime.runtimeMux);
    memcpy(runtime.pendingCmdSrc, src, 6);
    runtime.pendingCmdHeader = h;
    runtime.pendingCmd = p;
    runtime.pendingCmdReady = true;
    portEXIT_CRITICAL(&runtime.runtimeMux);
}
void verarbeiteAusstehende() {
    portENTER_CRITICAL(&runtime.runtimeMux);
    bool cmd = runtime.pendingCmdReady;
    SmartHome::CmdPayload cp = runtime.pendingCmd;
    SmartHome::MsgHeader ch = runtime.pendingCmdHeader;
    uint8_t cs[6]; memcpy(cs, runtime.pendingCmdSrc, 6);
    runtime.pendingCmdReady = false;
    portEXIT_CRITICAL(&runtime.runtimeMux);
    if (cmd) handleCmd(cs, ch, cp);
}
#endif

void handleCfgMsg(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CfgPayload& p) {
    if (!isMaster(s)) return;
    bool ok = handleCfg(p);
    if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
}

void handleEspNow(const uint8_t* s, const uint8_t* d, int len) {
    if (!s || !d || len < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(d, (size_t)len)) return;
    auto h = reinterpret_cast<const SmartHome::MsgHeader*>(d);
    const uint8_t* pl = d + SH_HEADER_SIZE;
    switch (h->msg_type) {
        case SH_MSG_HELLO_ACK:
            if (h->payload_len == sizeof(SmartHome::HelloAckPayload))
                handleHelloAck(s, *reinterpret_cast<const SmartHome::HelloAckPayload*>(pl));
            break;
        case SH_MSG_CMD:
            if (h->payload_len == sizeof(SmartHome::CmdPayload)) {
#if NET_ERL_USE_ISR_CMD_QUEUE
                merkePendingCmd(s, *h, *reinterpret_cast<const SmartHome::CmdPayload*>(pl));
#else
                handleCmd(s, *h, *reinterpret_cast<const SmartHome::CmdPayload*>(pl));
#endif
            }
            break;
        case SH_MSG_CFG:
            if (h->payload_len == sizeof(SmartHome::CfgPayload))
                handleCfgMsg(s, *h, *reinterpret_cast<const SmartHome::CfgPayload*>(pl));
            break;
        default: break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRcv(const esp_now_recv_info_t* i, const uint8_t* d, int l) { if (i) handleEspNow(i->src_addr, d, l); }
#else
void onEspNowRcv(const uint8_t* s, const uint8_t* d, int l) { handleEspNow(s, d, l); }
#endif

void onEspNowSent(const uint8_t*, esp_now_send_status_t s) {
    if (s != ESP_NOW_SEND_SUCCESS) logf("WARN", "ESP-NOW send fail");
}

void initFunk() {
    if (runtime.funk_bereit || runtime.setup_mode) return;
    WiFi.mode(WIFI_STA); WiFi.disconnect(); WiFi.setSleep(false);
    esp_wifi_set_channel(WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) { logf("WARN", "ESP-NOW init fail"); return; }
    esp_now_register_send_cb(onEspNowSent);
    esp_now_register_recv_cb(onEspNowRcv);
    runtime.funk_bereit = true; ensurePeer(BROADCAST_MAC);
    if (runtime.master_mac_gueltig) ensurePeer(runtime.master_mac);
}

// =============================================================================
// PRÄSENZ + AUTO-LIGHT
// =============================================================================

void motionOn(unsigned long j) {
    runtime.motion_aktiv = true;
    runtime.letzte_motion_ms = j;
    runtime.state_report_offen = true;
    runtime.pending_motion_event_state = 1U;
    runtime.blocked_by_lux = false;
    runtime.pending_auto_on_decision = false;
    if (!runtime.relay_1) {
        runtime.pending_auto_on_decision = true;  // Lux-check im Sensor-Poll
    }
}

void pollPresence(unsigned long j) {
    if (intervalElapsed(runtime.letztes_sensor_poll_ms, j, NET_ERL_SENSOR_POLL_INTERVAL_MS)) {
        runtime.letztes_sensor_poll_ms = j;
        bool high = netErlDeviceReadPresence();
        if (high) {
            if (!runtime.motion_aktiv) {
                motionOn(j);
            } else {
#if NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION
                runtime.letzte_motion_ms = j;  // Kitchen-Stil: Nachlauf verlängern
#endif
            }
            return;
        }
        unsigned long offMs = (unsigned long)runtime.auto_off_delay_s * 1000UL;
        if (runtime.motion_aktiv && runtime.letzte_motion_ms > 0 && (j - runtime.letzte_motion_ms) >= offMs) {
            runtime.motion_aktiv = false;
            runtime.blocked_by_lux = false;
            runtime.pending_auto_on_decision = false;
            runtime.state_report_offen = true;
            runtime.pending_motion_event_state = 2U;
            if (runtime.relay_1 && runtime.relay_auto_owned) {
                setRelay(false, "auto_off");
                sendRelayEvent(SH_TRIGGER_AUTO_OFF_TIMER);
                runtime.relay_auto_owned = false;
            }
        }
    }
}

#if defined(NET_ERL_HAS_BUTTON)
void processBtn(unsigned long j) {
    bool raw = netErlDeviceReadButton();
    if (raw != runtime.button_raw_active) { runtime.button_raw_active = raw; runtime.button_changed_at_ms = j; }
    if ((j - runtime.button_changed_at_ms) < BUTTON_DEBOUNCE_MS) return;
    if (raw == runtime.button_stable_active) return;
    runtime.button_stable_active = raw;
    if (raw) { runtime.button_pressed_at_ms = j; return; }
    unsigned long held = runtime.button_pressed_at_ms > 0 ? (j - runtime.button_pressed_at_ms) : 0;
    runtime.button_pressed_at_ms = 0;
    if (held >= SETUP_BUTTON_HOLD_MS) return;  // Langdruck → Setup, nicht toggeln
    runtime.relay_auto_owned = false; runtime.pending_auto_on_decision = false;
    runtime.blocked_by_lux = false;
    setRelay(!runtime.relay_1, "button");
    sendRelayEvent(SH_TRIGGER_MANUAL_BUTTON);
    runtime.state_report_offen = true;
}
#else
inline void processBtn(unsigned long) {}
#endif

// =============================================================================
// ARDUINO – setup()
// =============================================================================

void setup() {
    if (DEBUG_LOKAL_AKTIV) { Serial.begin(115200); delay(150); }
    initWdt();
    runtime = {};
    runtime.report_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.stored_sensor_send_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
    runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    runtime.state_report_offen = true;
    runtime.boot_ms = millis();

    netErlDeviceInit();
    netErlDeviceResetSensorDefaults();

    // Relais + Button initialisieren
    pinMode(PIN_RELAY_1, OUTPUT);
    netErlDeviceSetRelayOutput(false);
    runtime.relay_1 = false;
#ifdef PIN_BUTTON_1
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
#endif

    // Provisioning konfigurieren
    auto cfg = SmartHome::NetErlProvisioning::makeConfig(DEVICE_ID, STORAGE_NS,
        NET_ERL_DEFAULT_REPORT_INTERVAL_S, NET_ERL_DEFAULT_REPORT_INTERVAL_S,
        MIN_REPORT_INTERVAL_S, MAX_REPORT_INTERVAL_S);
    cfg.setupButtonPin = SETUP_BUTTON_PIN;
    cfg.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    cfg.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS;
    cfg.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    cfg.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0;
    cfg.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    runtime.provisioning_bereit = nodeProvisioning.begin(cfg,
        &runtime.master_mac_gueltig, runtime.master_mac,
        &runtime.report_interval_s, &runtime.stored_sensor_send_interval_s,
        &runtime.setup_mode, &runtime.setup_ap_aktiv, &runtime.restart_pending,
        &runtime.restart_requested_at_ms, runtime.setup_ap_ssid, sizeof(runtime.setup_ap_ssid),
        &provisioningHandler, provLog);

    if (!runtime.provisioning_bereit) { logf("WARN", "Prov init fail"); return; }

    setReportInt(nodeProvisioning.sanitizeStatusSendInterval(runtime.report_interval_s));
    runtime.stored_sensor_send_interval_s = nodeProvisioning.sanitizeSensorSendInterval(runtime.stored_sensor_send_interval_s);
    logf("INFO", "%s v%s (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "%s %s %s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);

    if (!nodeProvisioning.hasStoredMasterMac()) { nodeProvisioning.enterSetupMode(); return; }
    initFunk(); sendHello();
}

// =============================================================================
// ARDUINO – loop()
// =============================================================================

void loop() {
    esp_task_wdt_reset();
    nodeProvisioning.update();

    if (!runtime.provisioning_bereit || runtime.setup_mode) {
        delay(LOOP_INTERVAL_MS); return;
    }
    if (!runtime.funk_bereit) initFunk();

    unsigned long j = millis();

    // Gerätespezifische Abläufe
    pollPresence(j);
    processBtn(j);
    netErlDevicePollSensors(j);
#if NET_ERL_USE_ISR_CMD_QUEUE
    verarbeiteAusstehende();
#endif

    // Ausstehendes Motion-Event senden
    if (runtime.pending_motion_event_state != 0U && runtime.master_bekannt && runtime.master_mac_gueltig) {
        if (sendMotionEvent(runtime.pending_motion_event_state == 1U))
            runtime.pending_motion_event_state = 0U;
    }

    // Periodische Protokoll-Nachrichten
    if (!runtime.master_bekannt && (j - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS)
        sendHello();
    if (runtime.master_bekannt && (j - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS)
        sendHeartbeat();

    // STATE senden wenn nötig
    bool sf = runtime.master_bekannt && runtime.master_mac_gueltig &&
        (runtime.state_report_offen || runtime.letzter_state_ms == 0 ||
         (runtime.state_interval_ms > 0 && (j - runtime.letzter_state_ms) >= runtime.state_interval_ms));
    if (sf) sendState();

    // Snapshot-Log (Debug)
    if (intervalElapsed(runtime.letztes_snap_log_ms, j, SNAPSHOT_LOG_INTERVAL_MS) && DEBUG_LOKAL_AKTIV) {
        netErlDeviceLogSnapshot();
        runtime.letztes_snap_log_ms = j;
    }

    delay(LOOP_INTERVAL_MS);
}
