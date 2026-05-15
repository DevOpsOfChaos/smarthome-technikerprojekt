// =============================================================================
// main.cpp – NET-ERL Kitchen: Kuechenlicht mit Radar + Luftqualitaet
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_erl_kitchen/main.cpp
// Hardware:   ESP32-C3 + BME680 + VEML7700 + ENS160 + LD2410 + NeoPixel + Relais
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN: Kuechen-Arbeitslicht mit Praesenz- und Luftqualitaetsueberwachung]
// === EINSATZZWECK ===
//
// Pin-Belegung (siehe PinConfig.h):
//   I2C:       SDA=GPIO1, SCL=GPIO0 (BME680 0x76, VEML7700 0x10, ENS160 0x52)
//   LD2410:    OUT=GPIO7 (HIGH=Praesenz)
//   Button:    GPIO6 (active-LOW, 40ms Debounce)
//   NeoPixel:  GPIO8 (17 LEDs, folgen Relais)
//   Relais:    GPIO10 (active-HIGH)
//
// Besonderheiten:
//   - LD2410 Radar (digital OUT, kein UART-Parsing)
//   - NeoPixel-Ring als Relais-Anzeige
//   - I2C Takt = 10000Hz (langsam, stabil)
//   - STATE via ExtendedRelayComfortGasConfigStateReportPayload
//   - Sensor-Maske: THLPGAMXXX, Input-Maske: BXXXX
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
#include <string.h>
#include <math.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_NeoPixel.h>
#include <ScioSense_ENS160.h>

#ifndef ENS160_REG_TEMP_IN
#define ENS160_REG_TEMP_IN 0x13
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"

using SmartHome::clampToU16;
using SmartHome::clampHum01pct;

#include "../../basetypes/net_erl/NetErlProvisioning.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

// =============================================================================
// KONSTANTEN
// =============================================================================

constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-ERL";
constexpr char DATEI_VERSION[] = "0.5.0";
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

constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
constexpr const char* STORAGE_NS = "net_erl_kit";
constexpr const char* STORAGE_KEY = "kitchen_cfg_v1";
static_assert(sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE, "DEVICE_ID passt nicht in SSID-Puffer");
constexpr uint32_t MAGIC = 0x4B544331UL;
constexpr uint16_t STORAGE_VERSION = 1U;
constexpr uint32_t PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint32_t GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint16_t AIR_METRIC_UNGUELTIG = 0xFFFFU;
constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;
constexpr uint8_t LED_RING_HELLIGKEIT = 24U;
constexpr uint32_t TASK_WDT_TIMEOUT_S = 15UL;
constexpr uint16_t I2C_TIMEOUT_MS = 50U;

// =============================================================================
// GLOBALE OBJEKTE
// =============================================================================

Adafruit_BME680 bme680;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
ScioSense_ENS160 ens160Addr52(NET_ERL_ENS160_PRIMARY_ADDRESS);
ScioSense_ENS160 ens160Addr53(NET_ERL_ENS160_FALLBACK_ADDRESS);
ScioSense_ENS160* ens160 = nullptr;
Adafruit_NeoPixel ledRing(LED_RING_COUNT, PIN_LED_RING, NEO_GRB + NEO_KHZ800);

// =============================================================================
// STRUKTUREN – SensorState, KitchenRuntime, Persistenz
// =============================================================================

struct SensorState {
    int16_t temp_01c; uint16_t hum_01pct; uint16_t lux;
    uint32_t pressure_pa; uint32_t gas_ohm; uint16_t aqi;
    uint16_t tvoc_ppb; uint16_t eco2_ppm; bool motion; bool fault;
};

struct KitchenRuntime {
    bool provisioning_bereit, setup_mode, setup_ap_aktiv, restart_pending, funk_bereit;
    bool motion_aktiv, relay_1, fault, bme_ok, lux_ok, ens_ok, ld2410_raw;
    bool button_raw_active, button_stable_active, button_last_stable_active;
    bool ring_initialized, relay_auto_owned, blocked_by_lux, pending_auto_on_decision;
    uint8_t bme680_adresse, ens160_adresse, pending_motion_event_state;
    unsigned long restart_requested_at_ms, letztes_hello_ms, letzter_heartbeat_ms, letzter_state_ms;
    unsigned long letztes_sensor_poll_ms, letztes_env_sample_ms;
    unsigned long letzter_bme_recovery_ms, letzter_lux_recovery_ms, letzter_ens_recovery_ms;
    unsigned long letztes_snapshot_log_ms, boot_ms, letzter_ens_gueltig_ms;
    unsigned long button_changed_at_ms, button_pressed_at_ms, letzte_motion_ms, state_interval_ms;
    uint8_t bme680_gueltige_messungen, master_mac[6], naechste_seq;
    bool master_bekannt, master_mac_gueltig, state_report_offen;
    uint32_t report_interval_s, stored_sensor_send_interval_s;
    uint16_t auto_on_lux_threshold, auto_off_delay_s;
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE];
    SensorState sensor;
};

struct KitchenPersistedSetupData { uint32_t magic; uint16_t version; uint16_t reserved; uint16_t autoOnLuxThreshold; uint16_t autoOffDelayS; };
struct KitchenSnapshot { uint16_t auto_on_lux_threshold; uint16_t auto_off_delay_s; };

KitchenRuntime runtime = {};

// =============================================================================
// HILFSFUNKTIONEN
// =============================================================================

bool parseUInt(const char* t, uint32_t& v) {
    if (!t || !*t) return false; v = 0;
    for (const char* c = t; *c; ++c) { if (*c < '0' || *c > '9') return false; uint32_t d = *c - '0'; if (v > (0xFFFFFFFFUL - d) / 10UL) return false; v = v * 10 + d; }
    return true;
}

String htmlEscape(const String& s) {
    String e; e.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); ++i) { char c = s[i];
        switch (c) { case '&': e += "&amp;"; break; case '<': e += "&lt;"; break; case '>': e += "&gt;"; break; case '"': e += "&quot;"; break; case '\'': e += "&#39;"; break; default: e += c; }
    } return e;
}

void logf(const char* l, const char* f, ...) {
    if (!DEBUG_LOKAL_AKTIV) return; char m[224]; va_list a; va_start(a, f); vsnprintf(m, sizeof(m), f, a); va_end(a);
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void initWdt() {
    esp_err_t e = esp_task_wdt_init(TASK_WDT_TIMEOUT_S, true);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { logf("WARN", "WDT init err=%d", (int)e); return; }
    e = esp_task_wdt_add(nullptr);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) logf("WARN", "WDT add err=%d", (int)e);
}

void provLog(const char* l, const char* m) {
    if (!DEBUG_LOKAL_AKTIV || !l || !m) return; Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

void cpy(char* t, size_t s, const char* src) {
    if (!t || !s) return; if (!src) { t[0] = '\0'; return; } strncpy(t, src, s - 1); t[s - 1] = '\0';
}

bool isBroadcast(const uint8_t* m) { return m && memcmp(m, BROADCAST_MAC, 6) == 0; }
bool isMaster(const uint8_t* m) { return runtime.master_mac_gueltig && m && memcmp(m, runtime.master_mac, 6) == 0; }
const uint8_t* helloDst() { return runtime.master_mac_gueltig ? runtime.master_mac : BROADCAST_MAC; }
void setReportInt(uint32_t s) { runtime.report_interval_s = s; runtime.state_interval_ms = s * 1000UL; }

// mapAqi500 – ENS160-AQI (1-5) auf Skala 0-500 abbilden (0 = ungueltig)
uint16_t mapAqi500(uint16_t r) { return (r >= 1 && r <= ENS160_AQI_MAX_BASIC) ? (uint16_t)(r * 100U) : 0U; }
// encT – Temperatur in Kelvin * 64 als uint16 kodieren (fuer ENS160-Kompensation)
uint16_t encT(float c) { return (uint16_t)((c + 273.15f) * 64.0f); }
// encH – Relative Feuchte * 512 als uint16 kodieren (fuer ENS160-Kompensation)
uint16_t encH(float h) { return (uint16_t)(h * 512.0f); }

int writeEnsEnv(uint8_t a, float t, float h) {
    uint8_t b[4]; uint16_t te = encT(t), he = encH(h);
    b[0] = te & 0xFF; b[1] = (te >> 8) & 0xFF; b[2] = he & 0xFF; b[3] = (he >> 8) & 0xFF;
    Wire.beginTransmission(a); Wire.write(ENS160_REG_TEMP_IN); Wire.write(b, 4); return Wire.endTransmission();
}

// =============================================================================
// NEOPIXEL-RING – folgt Relais-Zustand
// =============================================================================

void updateRing(const char* grund) {
    if (!runtime.ring_initialized) { ledRing.begin(); ledRing.setBrightness(LED_RING_HELLIGKEIT); ledRing.clear(); ledRing.show(); runtime.ring_initialized = true; }
    if (runtime.relay_1) { uint32_t c = ledRing.Color(24, 24, 24); for (uint16_t i = 0; i < ledRing.numPixels(); ++i) ledRing.setPixelColor(i, c); }
    else ledRing.clear();
    ledRing.show();
    logf("INFO", "Ring folgt Relais (%s)", grund ? grund : "?");
}

// =============================================================================
// RELAIS
// =============================================================================

void setRelay(bool an, const char* g) {
    digitalWrite(PIN_RELAY_1, an == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
    runtime.relay_1 = an; updateRing(g);
    logf("INFO", "Relay %s (%s)", an ? "ON" : "OFF", g ? g : "?");
}

// =============================================================================
// PERSISTENZ
// =============================================================================

void snapK(Snapshot& s) { s.auto_on_lux_threshold = runtime.auto_on_lux_threshold; s.auto_off_delay_s = runtime.auto_off_delay_s; }
void restK(const KitchenSnapshot& s) { runtime.auto_on_lux_threshold = s.auto_on_lux_threshold; runtime.auto_off_delay_s = s.auto_off_delay_s; }
void snapBasis(SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, KitchenSnapshot& d) { nodeProvisioning.captureBasisSnapshot(b); snapK(d); }
void restBasis(const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& b, const KitchenSnapshot& d) { nodeProvisioning.restoreBasisSnapshot(b); restK(d); setReportInt(runtime.report_interval_s); }

// =============================================================================
// PROVISIONING-HANDLER
// =============================================================================

class KitchenProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
public:
    const char* pageTitle() const override { return "NET-ERL Kitchen"; }
    const char* pageIntro() const override { return "status_send_interval_s steuert STATE"; }
    const char* deviceSectionTitle() const override { return "Kitchen"; }
    const char* deviceSectionIntro() const override { return "Lux-Schwelle und Nachlauf."; }
    void loadDeviceDefaults() override { runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD; runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S; }
    bool loadDeviceSettings(Preferences& p) override {
        KitchenPersistedSetupData d = {};
        if (p.getBytesLength(STORAGE_KEY) != sizeof(d)) return false;
        if (p.getBytes(STORAGE_KEY, &d, sizeof(d)) != sizeof(d)) return false;
        if (d.magic != MAGIC || d.version != STORAGE_VERSION) return false;
        runtime.auto_on_lux_threshold = d.autoOnLuxThreshold; runtime.auto_off_delay_s = d.autoOffDelayS; return true;
    }
    bool saveDeviceSettings(Preferences& p) override {
        KitchenPersistedSetupData d = {}; d.magic = MAGIC; d.version = STORAGE_VERSION;
        d.autoOnLuxThreshold = runtime.auto_on_lux_threshold; d.autoOffDelayS = runtime.auto_off_delay_s;
        return p.putBytes(STORAGE_KEY, &d, sizeof(d)) == sizeof(d);
    }
    bool clearDeviceSettings(Preferences& p) override { p.remove(STORAGE_KEY); return true; }
    void captureDeviceSnapshot() override { snapK(s_); }
    void restoreDeviceSnapshot() override { restK(s_); }
    bool parseDeviceSave(WebServer& srv, String& err) override {
        p_ = {}; uint32_t v;
        if (!parseUInt(srv.arg("auto_on_lux_threshold").c_str(), v) || v > 65535UL) { err = "auto_on_lux_threshold ungueltig"; return false; }
        p_.auto_on_lux_threshold = (uint16_t)v;
        if (!parseUInt(srv.arg("auto_off_delay_s").c_str(), v) || v > 65535UL) { err = "auto_off_delay_s ungueltig"; return false; }
        p_.auto_off_delay_s = (uint16_t)v; p_.g = true; return true;
    }
    void applyParsedDeviceSettings() override { if (!p_.g) return; runtime.auto_on_lux_threshold = p_.auto_on_lux_threshold; runtime.auto_off_delay_s = p_.auto_off_delay_s; }
    void discardParsedDeviceSettings() override { p_ = {}; }
    void appendDeviceFieldsHtml(String& page, WebServer* src) const override {
        String lt = src && src->hasArg("auto_on_lux_threshold") ? src->arg("auto_on_lux_threshold") : String(runtime.auto_on_lux_threshold);
        String od = src && src->hasArg("auto_off_delay_s") ? src->arg("auto_off_delay_s") : String(runtime.auto_off_delay_s);
        page += "<div class=\"field\"><label>auto_on_lux_threshold</label><input name=\"auto_on_lux_threshold\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscape(lt) + "\"><div class=\"hint\">Lux</div></div>";
        page += "<div class=\"field\"><label>auto_off_delay_s</label><input name=\"auto_off_delay_s\" type=\"number\" min=\"0\" max=\"65535\" value=\"" + htmlEscape(od) + "\"><div class=\"hint\">Nachlauf (s)</div></div>";
    }
private:
    struct P { bool g = false; uint16_t auto_on_lux_threshold = 0; uint16_t auto_off_delay_s = 0; } p_{};
    KitchenSnapshot s_{};
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
KitchenProvisioningHandler kitchenProvisioningHandler;

// =============================================================================
// ESP-NOW INFRASTRUKTUR (identisch zu hall_light)
// =============================================================================

bool ensurePeer(const uint8_t* m) {
    if (!runtime.funk_bereit || !m) return false;
    if (!isBroadcast(m) && !SmartHome::isValidMac(m)) return false;
    if (esp_now_is_peer_exist(m)) return true;
    esp_now_peer_info_t p = {}; memcpy(p.peer_addr, m, 6); p.channel = WLAN_KANAL; p.encrypt = false;
    return esp_now_add_peer(&p) == ESP_OK;
}

bool sendPacketOpt(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb, uint8_t fl, uint8_t* seq) {
    if (!runtime.funk_bereit || !z || plen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!ensurePeer(z)) return false;
    uint8_t buf[SH_ESPNOW_MAX_BYTES] = {}; SmartHome::MsgHeader h = {};
    uint8_t s = runtime.naechste_seq++; SmartHome::fillHeader(h, mt, s, fl, plen);
    if (plen && pl) memcpy(buf + SH_HEADER_SIZE, pl, plen);
    SmartHome::finalizePacketCrc(h, buf + SH_HEADER_SIZE); memcpy(buf, &h, sizeof(h));
    if (esp_now_send(z, buf, SH_HEADER_SIZE + plen) != ESP_OK) { logf("WARN", "%s send fail", lb ? lb : "?"); return false; }
    if (seq) *seq = s; return true;
}
bool sendPacket(const uint8_t* z, uint8_t mt, const void* pl, size_t plen, const char* lb) { return sendPacketOpt(z, mt, pl, plen, lb, 0, nullptr); }
bool sendAck(const uint8_t* z, uint8_t s, uint8_t mt, uint8_t st) { SmartHome::AckPayload p = {}; p.ack_seq = s; p.ack_msg_type = mt; p.status = st; return sendPacket(z, SH_MSG_ACK, &p, sizeof(p), "ACK"); }

uint8_t autoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (runtime.lux_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    return f;
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN
// =============================================================================

bool sendHello() {
    SmartHome::HelloPayload p = {};
    cpy(p.device_id, sizeof(p.device_id), DEVICE_ID); cpy(p.device_name, sizeof(p.device_name), DEVICE_NAME);
    p.device_class = SH_CLASS_NET_ERL; p.caps_hi = (DEVICE_CAPS >> 8) & 0xFF; p.caps_lo = DEVICE_CAPS & 0xFF;
    p.power_type = SH_POWER_MAINS; p.fw_version = 1; p.boot_counter = BOOT_COUNTER;
    p.meta_schema_version = DEVICE_META_SCHEMA_VERSION; p.control_mode = DEVICE_CONTROL_MODE;
    p.config_profile = DEVICE_CONFIG_PROFILE; p.reporting_mode = DEVICE_REPORTING_MODE;
    cpy(p.sensor_mask, sizeof(p.sensor_mask), "THLPGAMXXX");
    cpy(p.input_mask, sizeof(p.input_mask), "BXXXX");
    runtime.letztes_hello_ms = millis();
    return sendPacket(helloDst(), SH_MSG_HELLO, &p, sizeof(p), "HELLO");
}

bool sendHeartbeat() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::HeartbeatPayload p = {}; cpy(p.node_id, sizeof(p.node_id), DEVICE_ID); p.uptime_s = millis() / 1000UL;
    if (!sendPacket(runtime.master_mac, SH_MSG_HEARTBEAT, &p, sizeof(p), "HEARTBEAT")) return false;
    runtime.letzter_heartbeat_ms = millis(); return true;
}

bool sendState() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::ExtendedRelayComfortGasConfigStateReportPayload p = {};
    cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.relay_1 = runtime.relay_1 ? 1U : 0U; p.temp_01c = runtime.sensor.temp_01c;
    p.hum_01pct = runtime.sensor.hum_01pct; p.lux = runtime.sensor.lux;
    p.pressure_pa = runtime.sensor.pressure_pa; p.gas_ohm = runtime.sensor.gas_ohm;
    p.aqi = runtime.sensor.aqi; p.tvoc_ppb = runtime.sensor.tvoc_ppb; p.eco2_ppm = runtime.sensor.eco2_ppm;
    p.motion = runtime.motion_aktiv ? 1U : 0U; p.auto_flags = autoFlags();
    p.fault = runtime.fault ? 1U : 0U; p.report_interval_s = (uint16_t)runtime.report_interval_s;
    p.auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    if (!sendPacket(runtime.master_mac, SH_MSG_STATE, &p, sizeof(p), "STATE")) return false;
    runtime.state_report_offen = false; runtime.letzter_state_ms = millis(); return true;
}

bool sendMotionEvent(bool s) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {}; cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_MOTION_DETECTED; p.trigger = SH_TRIGGER_AUTO; p.param1 = s ? 1U : 0U;
    return sendPacket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), s ? "EVT_M_ON" : "EVT_M_OFF");
}

bool sendRelayEvent(uint8_t tr) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {}; cpy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_RELAY_CHANGED; p.trigger = tr; p.param1 = runtime.relay_1 ? 1U : 0U;
    return sendPacket(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), "EVT_RELAY");
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG (vereinfacht – analog hall_light)
// =============================================================================

void handleHelloAck(const uint8_t* s, const SmartHome::HelloAckPayload& p) {
    if (p.ack_status != SH_ACK_OK || !runtime.master_mac_gueltig || !isMaster(s)) { logf("WARN", "HELLO_ACK fail"); return; }
    runtime.master_bekannt = true; runtime.state_report_offen = true; ensurePeer(runtime.master_mac);
}
void handleCmd(const uint8_t* s, const SmartHome::MsgHeader& h, const SmartHome::CmdPayload& p) {
    if (!isMaster(s)) return;
    if (p.cmd_type == SH_CMD_STATE_REQUEST) { runtime.state_report_offen = true; return; }
    if (p.cmd_type == SH_CMD_SET_RELAY) {
        if (p.param1 != 0U) { if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_REJECTED); return; }
        runtime.relay_auto_owned = false; runtime.pending_auto_on_decision = false; runtime.blocked_by_lux = false;
        setRelay(p.param2 != 0U, "master"); runtime.state_report_offen = true; sendRelayEvent(SH_TRIGGER_MASTER_CMD);
        if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_OK); return;
    }
    if (h.flags & SH_FLAG_ACK_REQUEST) sendAck(s, h.seq, h.msg_type, SH_ACK_REJECTED);
}

bool saveReportIntCfg(uint32_t v) {
    if (!nodeProvisioning.isSendIntervalValid(v)) return false;
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; KitchenSnapshot ds = {};
    snapBasis(bs, ds); setReportInt(v); runtime.state_report_offen = true;
    if (nodeProvisioning.saveCurrentState()) return true;
    restBasis(bs, ds); return false;
}

bool handleCfg(const SmartHome::CfgPayload& p) {
    switch (p.param_id) {
        case SH_CFG_REPORT_INTERVAL_S: return saveReportIntCfg(p.value);
        case SH_CFG_LIGHT_THRESHOLD_ON: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; KitchenSnapshot ds = {};
            snapBasis(bs, ds); runtime.auto_on_lux_threshold = (uint16_t)p.value; runtime.state_report_offen = true;
            if (nodeProvisioning.saveCurrentState()) return true; restBasis(bs, ds); return false;
        }
        case SH_CFG_AUTO_OFF_DELAY_S: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; KitchenSnapshot ds = {};
            snapBasis(bs, ds); runtime.auto_off_delay_s = (uint16_t)p.value; runtime.state_report_offen = true;
            if (nodeProvisioning.saveCurrentState()) return true; restBasis(bs, ds); return false;
        }
        default: return false;
    }
}

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
        case SH_MSG_HELLO_ACK: if (h->payload_len == sizeof(SmartHome::HelloAckPayload)) handleHelloAck(s, *reinterpret_cast<const SmartHome::HelloAckPayload*>(pl)); break;
        case SH_MSG_CMD: if (h->payload_len == sizeof(SmartHome::CmdPayload)) handleCmd(s, *h, *reinterpret_cast<const SmartHome::CmdPayload*>(pl)); break;
        case SH_MSG_CFG: if (h->payload_len == sizeof(SmartHome::CfgPayload)) handleCfgMsg(s, *h, *reinterpret_cast<const SmartHome::CfgPayload*>(pl)); break;
        default: break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRcv(const esp_now_recv_info_t* i, const uint8_t* d, int l) { if (i) handleEspNow(i->src_addr, d, l); }
#else
void onEspNowRcv(const uint8_t* s, const uint8_t* d, int l) { handleEspNow(s, d, l); }
#endif
void onEspNowSent(const uint8_t*, esp_now_send_status_t s) { if (s != ESP_NOW_SEND_SUCCESS) logf("WARN", "ESP-NOW send fail"); }

void initFunk() {
    if (runtime.funk_bereit || runtime.setup_mode) return;
    WiFi.mode(WIFI_STA); WiFi.disconnect(); WiFi.setSleep(false);
    esp_wifi_set_channel(WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) { logf("WARN", "ESP-NOW init fail"); return; }
    esp_now_register_send_cb(onEspNowSent); esp_now_register_recv_cb(onEspNowRcv);
    runtime.funk_bereit = true; ensurePeer(BROADCAST_MAC);
    if (runtime.master_mac_gueltig) ensurePeer(runtime.master_mac);
}

// =============================================================================
// SENSOR-INIT + SENSOR-LOGIK
// =============================================================================

void konfVeml() { veml.setGain(VEML7700_GAIN_1); veml.setIntegrationTime(VEML7700_IT_400MS); }
bool initBme() {
    uint8_t addrs[] = {(uint8_t)NET_ERL_BME680_PRIMARY_ADDRESS, (uint8_t)NET_ERL_BME680_FALLBACK_ADDRESS};
    for (uint8_t a : addrs) {
        if (!bme680.begin(a, &Wire)) continue;
        bme680.setTemperatureOversampling(BME680_OS_8X); bme680.setHumidityOversampling(BME680_OS_2X);
        bme680.setPressureOversampling(BME680_OS_4X); bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme680.setGasHeater(320U, 150U); runtime.bme680_adresse = a; return true;
    }
    return false;
}
bool initEns() {
    ens160 = &ens160Addr52; if (ens160->begin()) return true;
    ens160 = &ens160Addr53; if (ens160->begin()) return true;
    ens160 = nullptr; return false;
}

void initSensor() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL); Wire.setClock(NET_ERL_I2C_CLOCK_HZ); Wire.setTimeOut(I2C_TIMEOUT_MS);
    runtime.bme_ok = initBme(); if (!runtime.bme_ok) logf("WARN", "BME680 init fail");
    if (!veml.begin()) { runtime.lux_ok = false; logf("WARN", "VEML7700 init fail"); }
    else { runtime.lux_ok = true; konfVeml(); }
    runtime.ens_ok = initEns(); if (!runtime.ens_ok) { logf("WARN", "ENS160 init fail"); }
    else if (!ens160->setMode(ENS160_OPMODE_STD)) { runtime.ens_ok = false; logf("WARN", "ENS160 mode fail"); }
    pinMode(PIN_LD2410_OUT, INPUT);
}

void defSensor() {
    runtime.sensor.temp_01c = INT16_MIN; runtime.sensor.hum_01pct = 0xFFFFU; runtime.sensor.lux = 0xFFFFU;
    runtime.sensor.pressure_pa = PRESSURE_UNGUELTIG; runtime.sensor.gas_ohm = GAS_OHM_UNGUELTIG;
    runtime.sensor.aqi = AIR_METRIC_UNGUELTIG; runtime.sensor.tvoc_ppb = AIR_METRIC_UNGUELTIG; runtime.sensor.eco2_ppm = AIR_METRIC_UNGUELTIG;
    runtime.sensor.motion = false; runtime.sensor.fault = false;
}

// recovDue – Prueft ob ein erneuter Sensor-Wiederherstellungsversuch faellig ist (l==0 => immer)
bool recovDue(unsigned long l, unsigned long j) { return l == 0 || (j - l) >= SENSOR_RECOVERY_RETRY_INTERVAL_MS; }
void bmeRecov(unsigned long j) { if (runtime.bme_ok || !recovDue(runtime.letzter_bme_recovery_ms, j)) return; runtime.letzter_bme_recovery_ms = j; runtime.bme_ok = initBme(); }
void luxRecov(unsigned long j) { if (runtime.lux_ok || !recovDue(runtime.letzter_lux_recovery_ms, j)) return; runtime.letzter_lux_recovery_ms = j; runtime.lux_ok = veml.begin(); if (runtime.lux_ok) konfVeml(); }
void ensRecov(unsigned long j) { if (runtime.ens_ok || !recovDue(runtime.letzter_ens_recovery_ms, j)) return; runtime.letzter_ens_recovery_ms = j; runtime.ens_ok = initEns(); if (runtime.ens_ok) ens160->setMode(ENS160_OPMODE_STD); }

// gasWarmupOk – BME680-Gassensor ausreichend eingelaufen (Zeit + gueltige Messungen)?
bool gasWarmupOk(unsigned long j) { return (j - runtime.boot_ms) >= NET_ERL_BME680_GAS_WARMUP_MS && runtime.bme680_gueltige_messungen >= NET_ERL_BME680_GAS_WARMUP_MIN_READS; }
// ensStale – ENS160-Messwerte veraltet weil letzter gueltiger Wert zu lange zurueckliegt?
bool ensStale(unsigned long j) {
    if (!runtime.ens_ok || !ens160) return true;
    return runtime.letzter_ens_gueltig_ms > 0 && (j - runtime.letzter_ens_gueltig_ms) > NET_ERL_ENS160_STALE_TIMEOUT_MS;
}

void readEnv(unsigned long j) {
    if ((j - runtime.letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    runtime.letztes_env_sample_ms = j;
    bmeRecov(j); luxRecov(j); ensRecov(j);

    if (runtime.bme_ok && bme680.performReading()) {
        float t = bme680.temperature, h = bme680.humidity, p = bme680.pressure;
        uint32_t g = bme680.gas_resistance;
        if (isfinite(t) && isfinite(h) && isfinite(p) && h >= 0 && h <= 100 && p >= 30000 && p <= 110000) {
            runtime.sensor.temp_01c = (int16_t)lroundf(t * 10.0f); runtime.sensor.hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
            runtime.sensor.pressure_pa = (uint32_t)lroundf(p);
            if (runtime.bme680_gueltige_messungen < 255) runtime.bme680_gueltige_messungen++;
            runtime.sensor.gas_ohm = (gasWarmupOk(j) && g > 0) ? g : GAS_OHM_UNGUELTIG;
        } else { runtime.bme_ok = false; logf("WARN", "BME680 unplausibel"); }
    }

    if (runtime.lux_ok) {
        float l = veml.readLux(); if (!isnan(l) && l >= 0) runtime.sensor.lux = clampToU16((long)lroundf(l));
        else { runtime.lux_ok = false; logf("WARN", "VEML7700 read fail"); }
    }

    if (runtime.ens_ok && ens160 && runtime.bme_ok) {
        int r = writeEnsEnv(runtime.ens160_adresse, bme680.temperature, bme680.humidity);
        if (r != 0) logf("WARN", "ENS160 comp fail err=%d", r);
    }

    if (runtime.ens_ok && ens160 && ens160->measure(false)) {
        uint16_t aq5 = ens160->getAQI500(), aq = ens160->getAQI();
        uint16_t maq = (aq5 > 0 && aq5 <= 500) ? aq5 : mapAqi500(aq);
        if (maq > 0) { runtime.sensor.aqi = maq; runtime.sensor.tvoc_ppb = ens160->getTVOC(); runtime.sensor.eco2_ppm = ens160->geteCO2(); runtime.letzter_ens_gueltig_ms = j; }
    }

    if (ensStale(j)) { runtime.sensor.aqi = AIR_METRIC_UNGUELTIG; runtime.sensor.tvoc_ppb = AIR_METRIC_UNGUELTIG; runtime.sensor.eco2_ppm = AIR_METRIC_UNGUELTIG; }
    runtime.fault = !(runtime.bme_ok && runtime.lux_ok) || !runtime.ens_ok;
    runtime.sensor.fault = runtime.fault;

    if (runtime.motion_aktiv && runtime.pending_auto_on_decision && !runtime.relay_1 && runtime.sensor.lux != 0xFFFFU) {
        runtime.pending_auto_on_decision = false;
        if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) { runtime.relay_auto_owned = true; runtime.blocked_by_lux = false; setRelay(true, "auto_on_late_lux"); sendRelayEvent(SH_TRIGGER_AUTO); runtime.state_report_offen = true; }
        else { runtime.blocked_by_lux = true; runtime.state_report_offen = true; }
    }
}

// =============================================================================
// PRAESENZ (LD2410) + BUTTON
// =============================================================================

void motionOn(unsigned long j) {
    runtime.motion_aktiv = true; runtime.sensor.motion = true;
    runtime.letzte_motion_ms = j; runtime.state_report_offen = true;
    runtime.pending_motion_event_state = 1U;
    runtime.blocked_by_lux = false; runtime.pending_auto_on_decision = false;
    if (!runtime.relay_1) {
        if (runtime.sensor.lux == 0xFFFFU) runtime.pending_auto_on_decision = true;
        else if (runtime.sensor.lux <= runtime.auto_on_lux_threshold) { runtime.relay_auto_owned = true; setRelay(true, "auto_on_motion"); sendRelayEvent(SH_TRIGGER_AUTO); }
        else runtime.blocked_by_lux = true;
    }
}

void pollPresence(unsigned long j) {
    if ((j - runtime.letztes_sensor_poll_ms) < NET_ERL_SENSOR_POLL_INTERVAL_MS) return;
    runtime.letztes_sensor_poll_ms = j;
    bool high = digitalRead(PIN_LD2410_OUT) == HIGH; runtime.ld2410_raw = high;
    if (high) { if (!runtime.motion_aktiv) motionOn(j); else runtime.letzte_motion_ms = j; return; }
    unsigned long offMs = (unsigned long)runtime.auto_off_delay_s * 1000UL;
    if (runtime.motion_aktiv && runtime.letzte_motion_ms > 0 && (j - runtime.letzte_motion_ms) >= offMs) {
        runtime.motion_aktiv = false; runtime.sensor.motion = false; runtime.blocked_by_lux = false;
        runtime.pending_auto_on_decision = false; runtime.state_report_offen = true;
        runtime.pending_motion_event_state = 2U;
        if (runtime.relay_1 && runtime.relay_auto_owned) { setRelay(false, "auto_off"); sendRelayEvent(SH_TRIGGER_AUTO_OFF_TIMER); runtime.relay_auto_owned = false; }
    }
}

bool readBtn() {
#if BUTTON_1_ACTIVE_LOW
    return digitalRead(PIN_BUTTON_1) == LOW;
#else
    return digitalRead(PIN_BUTTON_1) == HIGH;
#endif
}

void processBtn(unsigned long j) {
    bool raw = readBtn();
    if (raw != runtime.button_raw_active) { runtime.button_raw_active = raw; runtime.button_changed_at_ms = j; }
    if ((j - runtime.button_changed_at_ms) < BUTTON_DEBOUNCE_MS) return;
    if (raw == runtime.button_stable_active) return;
    runtime.button_stable_active = raw;
    if (raw) { runtime.button_pressed_at_ms = j; return; }
    unsigned long held = runtime.button_pressed_at_ms > 0 ? (j - runtime.button_pressed_at_ms) : 0;
    runtime.button_pressed_at_ms = 0;
    if (held >= SETUP_BUTTON_HOLD_MS) return;
    runtime.relay_auto_owned = false; runtime.pending_auto_on_decision = false; runtime.blocked_by_lux = false;
    setRelay(!runtime.relay_1, "button"); sendRelayEvent(SH_TRIGGER_MANUAL_BUTTON); runtime.state_report_offen = true;
}

// =============================================================================
// ARDUINO – setup() und loop()
// =============================================================================

void setup() {
    if (DEBUG_LOKAL_AKTIV) { Serial.begin(115200); delay(150); }
    initWdt();
    runtime = {};
    runtime.report_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.stored_sensor_send_interval_s = NET_ERL_DEFAULT_REPORT_INTERVAL_S;
    runtime.auto_on_lux_threshold = NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD;
    runtime.auto_off_delay_s = NET_ERL_DEFAULT_AUTO_OFF_DELAY_S;
    runtime.state_report_offen = true; runtime.boot_ms = millis(); runtime.sensor.motion = false;
    defSensor();
    pinMode(PIN_RELAY_1, OUTPUT); setRelay(false, "boot"); pinMode(PIN_BUTTON_1, INPUT_PULLUP);

    auto cfg = SmartHome::NetErlProvisioning::makeConfig(DEVICE_ID, STORAGE_NS,
        NET_ERL_DEFAULT_REPORT_INTERVAL_S, NET_ERL_DEFAULT_REPORT_INTERVAL_S,
        MIN_REPORT_INTERVAL_S, MAX_REPORT_INTERVAL_S);
    cfg.setupButtonPin = SETUP_BUTTON_PIN; cfg.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    cfg.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS; cfg.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    cfg.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0; cfg.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    runtime.provisioning_bereit = nodeProvisioning.begin(cfg,
        &runtime.master_mac_gueltig, runtime.master_mac,
        &runtime.report_interval_s, &runtime.stored_sensor_send_interval_s,
        &runtime.setup_mode, &runtime.setup_ap_aktiv, &runtime.restart_pending,
        &runtime.restart_requested_at_ms, runtime.setup_ap_ssid, sizeof(runtime.setup_ap_ssid),
        &kitchenProvisioningHandler, provLog);
    if (!runtime.provisioning_bereit) { logf("WARN", "Prov init fail"); return; }

    setReportInt(nodeProvisioning.sanitizeStatusSendInterval(runtime.report_interval_s));
    runtime.stored_sensor_send_interval_s = nodeProvisioning.sanitizeSensorSendInterval(runtime.stored_sensor_send_interval_s);
    logf("INFO", "%s v%s (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "%s %s %s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);

    if (!nodeProvisioning.hasStoredMasterMac()) { nodeProvisioning.enterSetupMode(); return; }
    initSensor(); initFunk(); sendHello();
}

void loop() {
    esp_task_wdt_reset(); nodeProvisioning.update();
    if (!runtime.provisioning_bereit || runtime.setup_mode) { delay(LOOP_INTERVAL_MS); return; }
    if (!runtime.funk_bereit) initFunk();

    unsigned long j = millis();
    pollPresence(j); processBtn(j); readEnv(j);

    if (runtime.pending_motion_event_state != 0U && runtime.master_bekannt && runtime.master_mac_gueltig) {
        if (sendMotionEvent(runtime.pending_motion_event_state == 1U)) runtime.pending_motion_event_state = 0U;
    }

    if (!runtime.master_bekannt && (j - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) sendHello();
    if (runtime.master_bekannt && (j - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) sendHeartbeat();

    bool sf = runtime.master_bekannt && runtime.master_mac_gueltig &&
        (runtime.state_report_offen || runtime.letzter_state_ms == 0 ||
         (runtime.state_interval_ms > 0 && (j - runtime.letzter_state_ms) >= runtime.state_interval_ms));
    if (sf) sendState();

    if ((j - runtime.letztes_snapshot_log_ms) >= NET_ERL_SNAPSHOT_LOG_INTERVAL_MS && DEBUG_LOKAL_AKTIV) {
        logf("INFO", "snap t=%d h=%u l=%u p=%lu g=%lu a=%u tv=%u ec=%u m=%s r=%s",
            (int)runtime.sensor.temp_01c, runtime.sensor.hum_01pct, runtime.sensor.lux,
            (unsigned long)runtime.sensor.pressure_pa, (unsigned long)runtime.sensor.gas_ohm,
            runtime.sensor.aqi, runtime.sensor.tvoc_ppb, runtime.sensor.eco2_ppm,
            runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0");
        runtime.letztes_snapshot_log_ms = j;
    }
    delay(LOOP_INTERVAL_MS);
}
