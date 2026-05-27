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
//   NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION  (1=jede erkannte Bewegung setzt den Nachlauf zurueck, 0=nur erste Bewegung)
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
#define NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION 1   // 1=setzt Nachlauf bei erneuter Bewegung zurueck, 0=nicht
#endif
#ifndef NET_ERL_USE_ISR_CMD_QUEUE
#define NET_ERL_USE_ISR_CMD_QUEUE 0             // 0=direkte Verarbeitung, 1=ISR-safe Queue (Hall)
#endif
#ifndef NET_ERL_WDT_TIMEOUT_S
#define NET_ERL_WDT_TIMEOUT_S 15UL
#endif
#ifndef NET_ERL_MANUAL_ON_MAX_WITHOUT_MOTION_MS
#define NET_ERL_MANUAL_ON_MAX_WITHOUT_MOTION_MS 1800000UL  // 30 Minuten ohne Motion nach manuellem Einschalten.
#endif
#ifndef NET_ERL_MANUAL_ON_MOTION_PROBE_MS
#define NET_ERL_MANUAL_ON_MOTION_PROBE_MS 15000UL          // 15 Sekunden finales Motion-Prueffen.
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

using SmartHome::intervalElapsed;

// Forward-Deklaration – wird weiter unten als globale Variable definiert
extern SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;

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
constexpr uint32_t PERSISTED_MAGIC = NET_ERL_PERSISTED_MAGIC;

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
    bool manual_follow_motion_active, manual_follow_motion_seen, manual_follow_motion_probe_active;
    uint16_t auto_on_lux_threshold, auto_off_delay_s;
    unsigned long letzte_motion_ms, manual_follow_started_ms, manual_follow_probe_started_ms;

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

// Nur von net_erl_hall_module genutzt (ISR-sicherer Kommando-Ringpuffer).
#if NET_ERL_USE_ISR_CMD_QUEUE
    // ISR-safe CMD/CFG-Pending-Queue
    portMUX_TYPE runtimeMux = portMUX_INITIALIZER_UNLOCKED;
    bool pendingCmdReady;
    SmartHome::CmdPayload pendingCmd;
    SmartHome::MsgHeader pendingCmdHeader;
    uint8_t pendingCmdSrc[6];
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
extern bool netErlDeviceGetCachedLux(uint16_t* luxOut);

#ifdef NET_ERL_HAS_INDICATOR_UPDATE
extern void netErlDeviceUpdateIndicators(bool relayOn);
#else
inline void netErlDeviceUpdateIndicators(bool) {}
#endif

#ifdef NET_ERL_HAS_BUTTON
extern bool netErlDeviceReadButton();
#endif

// =============================================================================
// HILFSFUNKTIONEN
// =============================================================================

// =============================================================================
// parseUInt – Parse einen vorzeichenlosen Integer aus einem C-String.
//
// WAS: Konvertiert einen String aus Ziffern ('0'-'9') in uint32_t.
//      - Lehnt negative Zahlen, Leerzeichen, und nicht-numerische Zeichen ab
//      - Erkennt Overflow und returniert false
//
// PARAM text:  Eingabestring (nur Ziffern erlaubt)
// PARAM out:   Ergebnisvariable (wird nur bei Erfolg geschrieben)
// RETURN:      true wenn erfolgreich, false bei ungültiger Eingabe oder Overflow
// =============================================================================
bool parseUInt(const char* text, uint32_t& out) {
    if (!text || !*text) {
        return false;
    }

    out = 0;
    for (const char* c = text; *c; ++c) {
        // Nur Ziffern '0'-'9' erlaubt
        if (*c < '0' || *c > '9') {
            return false;
        }

        uint32_t digit = *c - '0';

        // Overflow-Pruefung: wuerde (out * 10 + digit) > UINT32_MAX?
        // Umgestellt: out > (UINT32_MAX - digit) / 10
        constexpr uint32_t UINT32_MAX_VAL = 0xFFFFFFFFUL;
        if (out > (UINT32_MAX_VAL - digit) / 10UL) {
            return false;  // Overflow
        }

        out = out * 10 + digit;
    }
    return true;
}

// =============================================================================
// htmlEscape – Ersetzt HTML-Sonderzeichen durch Entity-Codes.
//
// WAS: Schuetzt vor XSS in Provisioning-Web-UI Feldern.
//      Ersetzt: & < > " ' durch &amp; &lt; &gt; &quot; &#39;
//
// PARAM s: Eingabestring
// RETURN:  Escapter String
// =============================================================================
String htmlEscape(const String& s) {
    // +16 als Sicherheitspuffer fuer worst-case Escaping
    String escaped;
    escaped.reserve(s.length() + 16);

    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        switch (c) {
            case '&':  escaped += "&amp;";  break;
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&#39;";  break;
            default:   escaped += c;        break;
        }
    }
    return escaped;
}

// =============================================================================
// logMsg – Debug-Log-Ausgabe auf Serial (nur wenn DEBUG_LOKAL_AKTIV).
//
// FORMAT: "[LEVEL] Nachricht"
//
// PARAM level:   Log-Level-String (z.B. "INFO", "WARN", "ERROR")
// PARAM format:  printf-Formatstring
// PARAM ...:     Format-Argumente
// =============================================================================
void logMsg(const char* level, const char* format, ...) {
    if (!DEBUG_LOKAL_AKTIV) {
        return;
    }

    constexpr size_t LOG_BUFFER_SIZE = 224;
    char message[LOG_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// =============================================================================
// initWdt – Task-Watchdog initialisieren.
//
// WAS: Konfiguriert den ESP32 Task-Watchdog mit NET_ERL_WDT_TIMEOUT_S
//      Sekunden. Bei Timeout wird ein Panic-Reset ausgeloest.
//
// WARUM: Verhindert haengende Loops. loop() muss regelmaessig
//        esp_task_wdt_reset() aufrufen (geschieht in loop() Zeile 1).
// =============================================================================
void initWdt() {
    constexpr uint32_t MS_PER_SECOND = 1000UL;

    esp_task_wdt_config_t wdtConfig = {};
    wdtConfig.timeout_ms = NET_ERL_WDT_TIMEOUT_S * MS_PER_SECOND;
    wdtConfig.trigger_panic = true;

    esp_err_t err = esp_task_wdt_init(&wdtConfig);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        logMsg("WARN", "WDT init err=%d", (int)err);
        return;
    }

    err = esp_task_wdt_add(nullptr);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        logMsg("WARN", "WDT add err=%d", (int)err);
    }
}

void provLog(const char* l, const char* m) {
    if (!DEBUG_LOKAL_AKTIV || !l || !m) return;
    Serial.print("["); Serial.print(l); Serial.print("] "); Serial.println(m);
}

// =============================================================================
// safeStrCopy – Sicheres Kopieren eines C-Strings mit Puffergroessen-Pruefung.
//
// WAS: Kopiert src nach dest, garantiert Null-Terminierung.
//      - Wenn dest oder size ungültig: kein-op
//      - Wenn src NULL: schreibt leeren String ""
//      - Sonst: strncpy mit expliziter Null-Terminierung
//
// PARAM dest:       Zielpuffer
// PARAM destSize:   Groesse des Zielpuffers in Bytes (inkl. Platz fuer '\0')
// PARAM src:        Quellstring (darf NULL sein)
// =============================================================================
void safeStrCopy(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';  // Garantierte Null-Terminierung
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
    logMsg("INFO", "Relay %s (%s)", an ? "ON" : "OFF", g ? g : "?");
}

// =============================================================================
// PERSISTENZ-HILFEN
// =============================================================================
// =============================================================================
// snapshotDeviceConfig – Speichert die aktuellen Auto-Light-Werte in ein
//                        NetErlSnapshot (fuer Rollback bei Save-Fehler).
// =============================================================================
void snapshotDeviceConfig(NetErlSnapshot& outSnapshot) {
    outSnapshot.auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    outSnapshot.auto_off_delay_s = runtime.auto_off_delay_s;
}

// =============================================================================
// restoreDeviceConfig – Stellt die Auto-Light-Werte aus einem Snapshot wieder
//                       her (Rollback nach fehlgeschlagenem NVS-Speichern).
// =============================================================================
void restoreDeviceConfig(const NetErlSnapshot& snapshot) {
    runtime.auto_on_lux_threshold = snapshot.auto_on_lux_threshold;
    runtime.auto_off_delay_s = snapshot.auto_off_delay_s;
}

// =============================================================================
// snapshotBasisAndDevice – Speichert sowohl Provisioning-Basis als auch
//                          Device-Konfiguration (kombinierter Snapshot).
// =============================================================================
void snapshotBasisAndDevice(
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot& outBasis,
    NetErlSnapshot& outDevice
) {
    nodeProvisioning.captureBasisSnapshot(outBasis);
    snapshotDeviceConfig(outDevice);
}

// =============================================================================
// restoreBasisAndDevice – Stellt Basis + Device aus Snapshots wieder her.
//                         Setzt danach das Report-Intervall neu.
// =============================================================================
void restoreBasisAndDevice(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basis,
    const NetErlSnapshot& device
) {
    nodeProvisioning.restoreBasisSnapshot(basis);
    restoreDeviceConfig(device);
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
    void captureDeviceSnapshot() override { snapshotDeviceConfig(s_); }
    void restoreDeviceSnapshot() override { restoreDeviceConfig(s_); }

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

// =============================================================================
// sendPacketOpt – ESP-NOW-Paket bauen und senden (volle Kontrolle).
//
// WAS: Baut ein komplettes ESP-NOW-Paket aus Header + Payload + CRC
//      und sendet es an die Ziel-MAC.
//
// PAKET-AUFBAU:
//   [ MsgHeader (SH_HEADER_SIZE Bytes) | Payload (plen Bytes) ]
//   - Header wird NACH Payload+CRC kopiert, weil finalizePacketCrc()
//     den Header modifiziert (CRC-Wert).
//
// PARAM targetMac:   Ziel-MAC-Adresse (6 Bytes)
// PARAM msgType:     Message-Typ (SH_MSG_*)
// PARAM payload:     Zeiger auf Payload-Daten (kann NULL sein wenn plen==0)
// PARAM payloadLen:  Laenge des Payload in Bytes
// PARAM label:       Bezeichnung fuer Log-Ausgabe
// PARAM flags:       Header-Flags (z.B. SH_FLAG_ACK_REQUEST)
// PARAM outSeq:      Optional: Rueckgabe der verwendeten Sequenz-Nummer
// RETURN:            true wenn erfolgreich gesendet
// =============================================================================
bool sendPacketOpt(
    const uint8_t* targetMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label,
    uint8_t flags,
    uint8_t* outSeq
) {
    // Guard: Funk nicht bereit, ungültige Zieladresse, oder Payload zu gross
    if (!runtime.funk_bereit || !targetMac || payloadLen > SH_MAX_PAYLOAD_BYTES) {
        return false;
    }

    // Peer muss registriert sein
    if (!ensurePeer(targetMac)) {
        return false;
    }

    // Puffer und Header vorbereiten
    uint8_t packetBuf[SH_ESPNOW_MAX_BYTES] = {};
    SmartHome::MsgHeader header = {};

    // Sequenz-Nummer holen und hochzaehlen
    uint8_t seqNum = runtime.naechste_seq++;

    // Header fuellen
    SmartHome::fillHeader(header, msgType, seqNum, flags, payloadLen);

    // Payload in Puffer kopieren (hinter den Header-Bereich)
    if (payloadLen > 0 && payload) {
        memcpy(packetBuf + SH_HEADER_SIZE, payload, payloadLen);
    }

    // CRC berechnen (schreibt in den Header)
    SmartHome::finalizePacketCrc(header, packetBuf + SH_HEADER_SIZE);

    // Header an den Anfang des Puffers kopieren
    memcpy(packetBuf, &header, sizeof(header));

    // Senden
    esp_err_t sendResult = esp_now_send(targetMac, packetBuf, SH_HEADER_SIZE + payloadLen);
    if (sendResult != ESP_OK) {
        logMsg("WARN", "%s send fail", label ? label : "?");
        return false;
    }

    // Optional: Sequenz-Nummer an Aufrufer zurueckgeben
    if (outSeq) {
        *outSeq = seqNum;
    }
    return true;
}

// sendPacket – ESP-NOW-Paket senden (Standard-Parameter)
bool sendPacket(
    const uint8_t* targetMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label
) {
    return sendPacketOpt(targetMac, msgType, payload, payloadLen, label, 0, nullptr);
}

// =============================================================================
// sendPacketWithRetry – ESP-NOW-Paket mit Wiederholung bei Fehler senden.
//
// WAS: Sendet ein Paket und wiederholt es bis zu NET_ERL_ESPNOW_RETRY_COUNT
//      mal bei Misserfolg (Standard: 2 Retries, 50ms Pause).
//
// WARUM: ESP-NOW ist unzuverlaessig (kein ACK auf MAC-Layer). Retry
//        erhoeht die Zustellwahrscheinlichkeit fuer kritische Nachrichten.
//
// SEQUENZ-NUMMER: Wird VOR dem Senden dekrementiert (nicht inkrementiert),
//                 weil sendPacketOpt() sie danach wieder hochzaehlt.
//                 Bei 0 → 255 (uint8_t Wrap-Around).
// =============================================================================
#ifndef NET_ERL_ESPNOW_RETRY_COUNT
#define NET_ERL_ESPNOW_RETRY_COUNT 2
#endif
#ifndef NET_ERL_ESPNOW_RETRY_DELAY_MS
#define NET_ERL_ESPNOW_RETRY_DELAY_MS 50UL
#endif

bool sendPacketWithRetry(
    const uint8_t* targetMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label
) {
    // Sequenz-Nummer vorbereiten:
    // Sequenznummer sichern, damit Wiederholungen dieselbe Seq verwenden.
    // sendPacket() inkrementiert runtime.naechste_seq intern;
    // bei Misserfolg wird der gesicherte Wert wiederhergestellt.
    const uint8_t savedSeq = runtime.naechste_seq;

    for (int attempt = 0; attempt <= NET_ERL_ESPNOW_RETRY_COUNT; attempt++) {
        if (attempt > 0) {
            runtime.naechste_seq = savedSeq;  // Gleiche Seq fuer Retry.
        }
        if (sendPacket(targetMac, msgType, payload, payloadLen, label)) {
            return true;  // Erfolgreich gesendet
        }

        // Nicht beim letzten Versuch noch einmal warten
        if (attempt < NET_ERL_ESPNOW_RETRY_COUNT) {
            logMsg("WARN", "%s retry %d/%d",
                   label ? label : "?",
                   attempt + 1,
                   NET_ERL_ESPNOW_RETRY_COUNT);
            delay(NET_ERL_ESPNOW_RETRY_DELAY_MS);
        }
    }

    logMsg("ERROR", "%s failed after %d attempts",
           label ? label : "?",
           NET_ERL_ESPNOW_RETRY_COUNT + 1);
    return false;
}

// sendAck – ACK-Bestaetigung an Sender
bool sendAck(const uint8_t* targetMac, uint8_t seq, uint8_t msgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = seq;
    payload.ack_msg_type = msgType;
    payload.status = status;
    return sendPacket(targetMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN
// =============================================================================

bool sendHello() {
    SmartHome::HelloPayload p = {};
    safeStrCopy(p.device_id, sizeof(p.device_id), DEVICE_ID);
    safeStrCopy(p.device_name, sizeof(p.device_name), DEVICE_NAME);
    p.device_class = SH_CLASS_NET_ERL;
    p.caps_hi = (DEVICE_CAPS >> 8) & 0xFF; p.caps_lo = DEVICE_CAPS & 0xFF;
    p.power_type = SH_POWER_MAINS; p.fw_version = 1; p.boot_counter = BOOT_COUNTER;
    p.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    p.control_mode = DEVICE_CONTROL_MODE;
    p.config_profile = DEVICE_CONFIG_PROFILE; p.reporting_mode = DEVICE_REPORTING_MODE;
    safeStrCopy(p.sensor_mask, sizeof(p.sensor_mask), NET_ERL_SENSOR_MASK);
    safeStrCopy(p.input_mask, sizeof(p.input_mask), NET_ERL_INPUT_MASK);
    runtime.letztes_hello_ms = millis();
    return sendPacketWithRetry(helloDst(), SH_MSG_HELLO, &p, sizeof(p), "HELLO");
}

bool sendHeartbeat() {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::HeartbeatPayload p = {};
    safeStrCopy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.uptime_s = millis() / 1000UL;
    if (!sendPacketWithRetry(runtime.master_mac, SH_MSG_HEARTBEAT, &p, sizeof(p), "HEARTBEAT")) return false;
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
    if (!sendPacketWithRetry(runtime.master_mac, SH_MSG_STATE, buf, ps, "STATE")) {
        logMsg("WARN", "STATE not sent payload=%u", (unsigned)ps);
        return false;
    }
    logMsg("INFO", "STATE sent payload=%u", (unsigned)ps);
    runtime.state_report_offen = false; runtime.letzter_state_ms = millis(); return true;
}

bool sendMotionEvent(bool s) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    safeStrCopy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_MOTION_DETECTED; p.trigger = SH_TRIGGER_AUTO;
    p.param1 = s ? 1U : 0U; p.param2 = 0U;
    return sendPacketWithRetry(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), s ? "EVT_M_ON" : "EVT_M_OFF");
}

bool sendRelayEvent(uint8_t tr) {
    if (!runtime.master_bekannt || !runtime.master_mac_gueltig) return false;
    SmartHome::EventReportPayload p = {};
    safeStrCopy(p.node_id, sizeof(p.node_id), DEVICE_ID);
    p.event_type = SH_EVENT_RELAY_CHANGED; p.trigger = tr;
    p.param1 = runtime.relay_1 ? 1U : 0U; p.param2 = 0U;
    return sendPacketWithRetry(runtime.master_mac, SH_MSG_EVENT, &p, sizeof(p), "EVT_RELAY");
}

// =============================================================================
// MANUELLES EINSCHALTEN MIT MOTION-FOLGELOGIK
// =============================================================================

// Aufgabe: Beendet den Modus "manuell eingeschaltet, danach Motion folgen".
// Eingabewerte:
// - reason beschreibt nur fuer Debug-Logs, warum der Modus beendet wurde.
// Ausgabewert: keiner; alle zugehoerigen Runtime-Flags werden geloescht.
//
// Dieser Modus wird fuer Server-AN und kurzen Button-AN genutzt. Server- oder
// Button-AUS bricht ihn sofort ab. Auto-Light nutzt weiterhin relay_auto_owned
// und bleibt dadurch sauber getrennt.
void cancelManualFollowMotion(const char* reason) {
    if (runtime.manual_follow_motion_active || runtime.manual_follow_motion_seen || runtime.manual_follow_motion_probe_active) {
        logMsg("INFO", "Manual-follow stop (%s)", reason ? reason : "?");
    }
    runtime.manual_follow_motion_active = false;
    runtime.manual_follow_motion_seen = false;
    runtime.manual_follow_motion_probe_active = false;
    runtime.manual_follow_started_ms = 0UL;
    runtime.manual_follow_probe_started_ms = 0UL;
}

// Aufgabe: Startet den Modus fuer manuelles Einschalten, das spaeter Motion folgt.
// Eingabewerte:
// - nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// - source beschreibt den Ausloeser im Debug-Log, z.B. "master" oder "button".
// Ausgabewert: keiner; der Modus bleibt aktiv, solange das Relais an ist.
//
// Verhalten:
// - Wenn der Praesenz-Pin beim Einschalten bereits HIGH ist, gilt Motion als gesehen.
//   Danach schaltet die Lampe aus, sobald Motion weg ist und die Nachlaufzeit
//   abgelaufen ist.
// - Wenn beim Einschalten keine Motion aktiv war, bleibt die Lampe an und wartet
//   auf die erste Motion. Ab dieser ersten Motion gilt wieder "Motion weg plus
//   Nachlaufzeit".
// - Wenn 30 Minuten lang keine Motion gesehen wurde, startet ein 15-Sekunden-
//   Prueffenster. Kommt in diesem Fenster kein HIGH vom Praesenzsensor, wird
//   die Lampe ausgeschaltet.
void startManualFollowMotion(unsigned long nowMs, const char* source) {
    const bool motionPinHighNow = netErlDeviceReadPresence();

    runtime.relay_auto_owned = false;
    runtime.pending_auto_on_decision = false;
    runtime.blocked_by_lux = false;
    runtime.manual_follow_motion_active = true;
    runtime.manual_follow_motion_seen = motionPinHighNow;
    runtime.manual_follow_motion_probe_active = false;
    runtime.manual_follow_started_ms = nowMs;
    runtime.manual_follow_probe_started_ms = 0UL;

    if (motionPinHighNow) {
        runtime.motion_aktiv = true;
        runtime.letzte_motion_ms = nowMs;
        runtime.pending_motion_event_state = 1U;
    }

    logMsg("INFO", "Manual-follow start (%s, motion=%s)",
        source ? source : "?",
        runtime.manual_follow_motion_seen ? "seen" : "wait");
}

// Aufgabe: Merkt, dass nach manuellem Einschalten mindestens einmal Motion erkannt wurde.
// Eingabewert: nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; ab jetzt gilt die normale Motion-weg-plus-Nachlauf-Regel.
//
// Wichtig: Diese Funktion wird auch im 15-Sekunden-Prueffenster genutzt. Ein
// einzelnes HIGH reicht, um zu beweisen, dass der Radar noch sinnvoll meldet.
void markManualFollowMotionSeen(unsigned long nowMs) {
    if (!runtime.manual_follow_motion_active) {
        return;
    }
    if (!runtime.manual_follow_motion_seen || runtime.manual_follow_motion_probe_active) {
        logMsg("INFO", "Manual-follow motion seen");
    }
    runtime.manual_follow_motion_seen = true;
    runtime.manual_follow_motion_probe_active = false;
    runtime.manual_follow_probe_started_ms = 0UL;
    runtime.letzte_motion_ms = nowMs;
}

// Aufgabe: Schaltet ein manuell eingeschaltetes Relais durch die Motion-Folgelogik aus.
// Eingabewert: reason beschreibt den Ausloeser fuer Debug-Logs und setRelay().
// Ausgabewert: keiner; Relais wird ausgeschaltet und ein Auto-Off-Event gemeldet.
//
// Der Trigger bleibt SH_TRIGGER_AUTO_OFF_TIMER, weil nicht Master oder Button
// ausschalten, sondern die interne Nachlauf-/Sicherheitslogik.
void turnOffManualFollowRelay(const char* reason) {
    setRelay(false, reason ? reason : "manual_follow_off");
    sendRelayEvent(SH_TRIGGER_AUTO_OFF_TIMER);
    cancelManualFollowMotion(reason);
    runtime.state_report_offen = true;
}

// Aufgabe: Sichert ein explizites AUS gegen sofortiges Wieder-Einschalten durch
// einen bereits HIGH stehenden Praesenzsensor.
// Eingabewert: nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; bei aktuellem Motion-HIGH wird runtime.motion_aktiv gesetzt.
//
// Warum das noetig ist:
// Server-AUS und Button-AUS sollen echte AUS-Entscheidungen sein. Wenn der
// Radar-Ausgang in genau diesem Moment HIGH ist, wuerde der naechste Poll sonst
// "neue Bewegung" sehen und Auto-Light sofort wieder einschalten. Stattdessen
// wird die aktuelle HIGH-Phase als bereits bekannt markiert. Erst nachdem der
// Sensor LOW war und spaeter wieder HIGH wird, darf Auto-Light erneut starten.
void holdExplicitOffUntilNextMotionRisingEdge(unsigned long nowMs) {
    if (!netErlDeviceReadPresence()) {
        return;
    }
    runtime.motion_aktiv = true;
    runtime.letzte_motion_ms = nowMs;
    runtime.pending_motion_event_state = 1U;
}

// Aufgabe: Sicherheitslogik fuer manuell eingeschaltetes Licht ohne jemals erkannte Motion.
// Eingabewert: nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: true bedeutet, die Funktion hat ausgeschaltet oder wartet im
// Prueffenster; der normale Motion-Off-Teil soll dann nichts weiter tun.
//
// Hintergrund: Wenn Server oder Button die Lampe einschalten und der Radar nie
// HIGH meldet, wuerde die Lampe sonst unbegrenzt an bleiben. Nach 30 Minuten
// startet deshalb ein 15-Sekunden-Fenster. Wird in diesem Fenster weiterhin
// keine Motion erkannt, wird ausgeschaltet.
bool handleManualFollowNoMotionWatchdog(unsigned long nowMs) {
    if (!runtime.manual_follow_motion_active || runtime.manual_follow_motion_seen || !runtime.relay_1) {
        return false;
    }

    if (!runtime.manual_follow_motion_probe_active) {
        const bool maxWithoutMotionElapsed = runtime.manual_follow_started_ms > 0UL
            && (nowMs - runtime.manual_follow_started_ms) >= NET_ERL_MANUAL_ON_MAX_WITHOUT_MOTION_MS;
        if (!maxWithoutMotionElapsed) {
            return false;
        }
        runtime.manual_follow_motion_probe_active = true;
        runtime.manual_follow_probe_started_ms = nowMs;
        logMsg("WARN", "Manual-follow probe started");
        return true;
    }

    const bool probeElapsed = runtime.manual_follow_probe_started_ms > 0UL
        && (nowMs - runtime.manual_follow_probe_started_ms) >= NET_ERL_MANUAL_ON_MOTION_PROBE_MS;
    if (!probeElapsed) {
        return true;
    }

    turnOffManualFollowRelay("manual_follow_no_motion");
    return true;
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG
// =============================================================================

void handleHelloAck(const uint8_t* s, const SmartHome::HelloAckPayload& p) {
    if (p.ack_status != SH_ACK_OK) { logMsg("WARN", "HELLO_ACK status=%d", (int)p.ack_status); return; }
    if (!runtime.master_mac_gueltig) { logMsg("WARN", "HELLO_ACK: master MAC nicht gueltig"); return; }
    if (!isMaster(s)) { logMsg("WARN", "HELLO_ACK: Absender ist nicht Master"); return; }
    runtime.master_bekannt = true; runtime.state_report_offen = true;
    ensurePeer(runtime.master_mac);
    logMsg("INFO", "HELLO_ACK accepted, STATE pending");
}

// =============================================================================
// handleCmd – ESP-NOW CMD-Nachricht vom Master verarbeiten.
//
// WAS: Verarbeitet zwei Befehlstypen:
//      - SH_CMD_STATE_REQUEST: Master fordert STATE-Update an
//      - SH_CMD_SET_RELAY:     Master schaltet Relais manuell
//
// WARUM: Ermöglicht dem Master, Geraet fernzusteuern.
//
// PARAM senderMac: MAC-Adresse des Senders (6 Bytes)
// PARAM header:    ESP-NOW Message-Header (Seq-Nummer, Flags, Typ)
// PARAM payload:   CMD-Payload (cmd_type, param1, param2)
//
// param1 bei SET_RELAY: Subcommand (0 = ein/ausschalten, !=0 = abgelehnt)
// param2 bei SET_RELAY: 0 = Relais AUS, !=0 = Relais AN
// =============================================================================
void handleCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    // Nur Befehle vom Master akzeptieren
    if (!isMaster(senderMac)) {
        return;
    }

    // --- STATE ANFORDERUNG ---
    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        runtime.state_report_offen = true;
        return;
    }

    // --- HELLO_REQUEST: Master fordert ein echtes HELLO vom Node ---
    if (payload.cmd_type == SH_CMD_HELLO_REQUEST) {
        // sendHello() sendet ein HELLO und setzt letzteres Hello-Timestamp
        sendHello();
        return;
    }

    // --- RELAIS SCHALTEN ---
    if (payload.cmd_type == SH_CMD_SET_RELAY) {
        // Subcommand != 0 ist nicht unterstuetzt → ablehnen
        if (payload.param1 != 0U) {
            if (header.flags & SH_FLAG_ACK_REQUEST) {
                sendAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
            }
            return;
        }

        // Relais schalten (param2 != 0 → AN, sonst AUS)
        bool relayOn = (payload.param2 != 0U);
        setRelay(relayOn, "master");
        if (relayOn) {
            startManualFollowMotion(millis(), "master");
        } else {
            runtime.relay_auto_owned = false;
            runtime.pending_auto_on_decision = false;
            runtime.blocked_by_lux = false;
            cancelManualFollowMotion("master_off");
            holdExplicitOffUntilNextMotionRisingEdge(millis());
        }

        runtime.state_report_offen = true;
        sendRelayEvent(SH_TRIGGER_MASTER_CMD);

        if (header.flags & SH_FLAG_ACK_REQUEST) {
            sendAck(senderMac, header.seq, header.msg_type, SH_ACK_OK);
        }
        return;
    }

    // Unbekannter Befehl → ablehnen
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
    }
}

bool saveReportIntCfg(uint32_t v) {
    if (!nodeProvisioning.isSendIntervalValid(v)) return false;
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot bs = {}; NetErlSnapshot ds = {};
    snapshotBasisAndDevice(bs, ds); setReportInt(v); runtime.state_report_offen = true;
    if (nodeProvisioning.saveCurrentState()) return true;
    restoreBasisAndDevice(bs, ds); return false;
}

bool saveDeviceCfgWithRollback(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const NetErlSnapshot& deviceSnapshot
) {
    runtime.state_report_offen = true;
    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }
    restoreBasisAndDevice(basisSnapshot, deviceSnapshot);
    return false;
}

// =============================================================================
// handleCfg – ESP-NOW CFG-Nachricht verarbeiten (Konfigurationsaenderung).
//
// WAS: Aendert eine Konfiguration und speichert sie persistent in NVS.
//      Verwendet Snapshot-Rollback: Bei Save-Fehler wird der alte
//      Zustand wiederhergestellt.
//
// PARAM payload: CFG-Payload mit param_id und neuem Wert
// RETURN:        true wenn erfolgreich gespeichert, false bei Fehler
// =============================================================================
bool handleCfg(const SmartHome::CfgPayload& payload) {
    switch (payload.param_id) {

        // --- Report-Intervall aendern ---
        case SH_CFG_REPORT_INTERVAL_S:
            return saveReportIntCfg(payload.value);

        // --- Lux-Schwelle fuer Auto-ON aendern ---
        case SH_CFG_LIGHT_THRESHOLD_ON: {
            // Alten Zustand sichern (fuer Rollback bei Fehler)
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
            NetErlSnapshot deviceSnapshot = {};
            snapshotBasisAndDevice(basisSnapshot, deviceSnapshot);

            // Neuen Wert setzen
            runtime.auto_on_lux_threshold = (uint16_t)payload.value;
            return saveDeviceCfgWithRollback(basisSnapshot, deviceSnapshot);
        }

        // --- Auto-OFF-Nachlaufzeit aendern ---
        case SH_CFG_AUTO_OFF_DELAY_S: {
            SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
            NetErlSnapshot deviceSnapshot = {};
            snapshotBasisAndDevice(basisSnapshot, deviceSnapshot);

            runtime.auto_off_delay_s = (uint16_t)payload.value;
            return saveDeviceCfgWithRollback(basisSnapshot, deviceSnapshot);
        }

        default:
            return false;  // Unbekannte param_id
    }
}

// Nur von net_erl_hall_module genutzt (ISR-sicherer Kommando-Ringpuffer).
#if NET_ERL_USE_ISR_CMD_QUEUE
// =============================================================================
// merkePendingCmd – CMD im ISR-safe Puffer speichern.
//
// WARUM: ESP-NOW-Callback laeuft im Interrupt-Kontext. handleCmd() darf
//        dort NICHT aufgerufen werden (nutzt Serial, NVS, delay).
//        Stattdessen: CMD in Puffer kopieren, in loop() verarbeiten.
// =============================================================================
void merkePendingCmd(const uint8_t* srcMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    portENTER_CRITICAL(&runtime.runtimeMux);
    memcpy(runtime.pendingCmdSrc, srcMac, 6);
    runtime.pendingCmdHeader = header;
    runtime.pendingCmd = payload;
    runtime.pendingCmdReady = true;
    portEXIT_CRITICAL(&runtime.runtimeMux);
}

// =============================================================================
// verarbeiteAusstehende – Gespeicherte CMDs aus ISR-Puffer in loop()
//                         verarbeiten. Kopiert Daten AUS dem Critical
//                         Section, dann aufruf von handleCmd() im
//                         sicheren Loop-Kontext.
// =============================================================================
void verarbeiteAusstehende() {
    // Daten atomar aus dem ISR-Puffer lesen
    portENTER_CRITICAL(&runtime.runtimeMux);
    bool hasPendingCmd = runtime.pendingCmdReady;
    SmartHome::CmdPayload cmdCopy = runtime.pendingCmd;
    SmartHome::MsgHeader headerCopy = runtime.pendingCmdHeader;
    uint8_t srcMacCopy[6];
    memcpy(srcMacCopy, runtime.pendingCmdSrc, 6);
    runtime.pendingCmdReady = false;
    portEXIT_CRITICAL(&runtime.runtimeMux);

    // Verarbeitung im sicheren Loop-Kontext
    if (hasPendingCmd) {
        handleCmd(srcMacCopy, headerCopy, cmdCopy);
    }
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

void onEspNowSent(const wifi_tx_info_t*, esp_now_send_status_t s) {
    if (s != ESP_NOW_SEND_SUCCESS) logMsg("WARN", "ESP-NOW send fail");
}

void initFunk() {
    if (runtime.funk_bereit || runtime.setup_mode) return;
    static uint8_t espNowInitFails = 0;
    constexpr uint8_t MAX_ESPNOW_INIT_FAILURES = 5;

    WiFi.mode(WIFI_STA); WiFi.disconnect(); WiFi.setSleep(false);
    esp_wifi_set_channel(WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        espNowInitFails++;
        logMsg("WARN", "ESP-NOW init fail (%u/%u)", espNowInitFails, MAX_ESPNOW_INIT_FAILURES);
        if (espNowInitFails >= MAX_ESPNOW_INIT_FAILURES) {
            logMsg("ERROR", "ESP-NOW init nach %u Versuchen fehlgeschlagen, restart", MAX_ESPNOW_INIT_FAILURES);
            ESP.restart();
        }
        return;
    }
    espNowInitFails = 0;  // Reset on success
    esp_now_register_send_cb(onEspNowSent);
    esp_now_register_recv_cb(onEspNowRcv);
    runtime.funk_bereit = true; ensurePeer(BROADCAST_MAC);
    if (runtime.master_mac_gueltig) ensurePeer(runtime.master_mac);
}

// =============================================================================
// PRÄSENZ + AUTO-LIGHT
// =============================================================================

// =============================================================================
// tryAutoOnFromCachedLux – Sofortige Auto-On-Entscheidung mit letztem Luxwert.
//
// Aufgabe: Prueft beim ersten Motion-HIGH sofort den letzten gueltigen Luxwert.
// Eingabewerte: keine; Lux kommt ueber netErlDeviceGetCachedLux() aus dem Device.
// Ausgabewert: true bedeutet, die Auto-On-Entscheidung ist erledigt.
//
// Warum diese Funktion existiert:
// Der Umwelt-/Lux-Poll laeuft deutlich langsamer als der Praesenz-Poll. Beim
// LED-Ring-Modul sind es 2500 Millisekunden fuer die Umweltsensoren, aber nur
// 50 Millisekunden fuer den LD2410. Ohne gecachten Luxwert wuerde eine erkannte
// Bewegung bis zum naechsten Lux-Poll warten. Das ist fuer Lichtsteuerung zu
// traege. Deshalb wird der zuletzt gemessene Luxwert sofort verwendet. Nur wenn
// noch kein gueltiger Luxwert vorhanden ist, bleibt pending_auto_on_decision
// gesetzt und die alte Late-Lux-Logik im Device darf spaeter entscheiden.
// =============================================================================
bool tryAutoOnFromCachedLux() {
    uint16_t cachedLux = 0xFFFFU;
    if (!netErlDeviceGetCachedLux(&cachedLux) || cachedLux == 0xFFFFU) {
        return false;
    }

    runtime.pending_auto_on_decision = false;
    if (cachedLux <= runtime.auto_on_lux_threshold) {
        runtime.relay_auto_owned = true;
        runtime.blocked_by_lux = false;
        setRelay(true, "auto_on_cached_lux");
        sendRelayEvent(SH_TRIGGER_AUTO);
        runtime.state_report_offen = true;
    } else {
        runtime.blocked_by_lux = true;
        runtime.state_report_offen = true;
    }
    return true;
}

// =============================================================================
// motionOn – Wird aufgerufen wenn Bewegung erkannt wird.
//
// WAS: Setzt den Runtime-Zustand auf "Praesenz erkannt".
// WARUM: Zentrales Update aller Praesenz-Flags fuer Auto-Light-Logik.
//
// PARAM nowMs:  Aktuelle Zeit in Millisekunden (millis()).
//
// NEBENEFFEKT: Wenn das Relais noch AUS ist, wird Auto-On sofort mit dem letzten
//              gueltigen Luxwert entschieden. Nur wenn kein Luxwert vorliegt,
//              bleibt pending_auto_on_decision fuer die Late-Lux-Logik gesetzt.
// =============================================================================
void motionOn(unsigned long nowMs) {
    runtime.motion_aktiv = true;                    // Bewegung ist jetzt aktiv
    runtime.letzte_motion_ms = nowMs;               // Zeitstempel fuer Nachlauf-Timer
    runtime.state_report_offen = true;              // STATE-Nachricht beim Master anfordern
    runtime.pending_motion_event_state = 1U;        // 1 = Motion-ON Event steht aus
    runtime.blocked_by_lux = false;                 // Lux-Sperre zuruecksetzen
    runtime.pending_auto_on_decision = false;       // Vorherige Auto-ON-Entscheidung loeschen

    markManualFollowMotionSeen(nowMs);

    // Wenn Relais AUS: sofort mit dem zuletzt gemessenen Luxwert entscheiden.
    // Ist noch kein Luxwert vorhanden, entscheidet die Device-Late-Lux-Logik
    // beim naechsten gueltigen Sensorwert.
    if (!runtime.relay_1) {
        runtime.pending_auto_on_decision = true;
        tryAutoOnFromCachedLux();
    }
}

// =============================================================================
// pollPresence – Praesenzsensor abfragen und Auto-Light-Nachlauf steuern.
//
// WAS: Liest den Bewegungssensor (via Device-Hook) und entscheidet:
//      - Bei neuer Bewegung: motionOn() ausloesen
//      - Bei wiederholter Bewegung: Nachlauf-Timer je nach Device-Konfig zuruecksetzen
//      - Bei Ablauf der Nachlaufzeit: Motion-Off, Relais ausschalten
//
// WARUM: Kern der Auto-Light-Logik. Verbindet Praesenz-Erkennung mit
//        Relais-Steuerung und ESP-NOW-Event-Meldungen.
//
// PARAM nowMs: Aktuelle Zeit in Millisekunden (millis()).
//
// KONFIG: NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION
//         1 = Jede erkannte Bewegung setzt den Nachlauf-Timer zurueck
//         0 = Nur die erste Bewegung startet den Nachlauf-Timer
// =============================================================================
void pollPresence(unsigned long nowMs) {
    // Sensor nur im konfigurierten Intervall abfragen
    if (!intervalElapsed(runtime.letztes_sensor_poll_ms, nowMs, NET_ERL_SENSOR_POLL_INTERVAL_MS)) {
        return;
    }
    runtime.letztes_sensor_poll_ms = nowMs;

    bool motionDetected = netErlDeviceReadPresence();

    if (motionDetected) {
        markManualFollowMotionSeen(nowMs);
        // --- BEWEGUNG ERKANNT ---
        if (!runtime.motion_aktiv) {
            // Erste Bewegung seit langer Zeit → Motion-ON ausloesen
            motionOn(nowMs);
        } else {
            // Bewegung dauert noch an
#if NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION
            // Nachlauf-Timer bei jeder erneuten Erkennung zuruecksetzen.
            runtime.letzte_motion_ms = nowMs;
#endif
        }
        return;  // Kein Motion-Off-Check waehrend Bewegung
    }

    if (handleManualFollowNoMotionWatchdog(nowMs)) {
        return;
    }

    // --- KEINE BEWEGUNG → Nachlauf-Timer pruefen ---
    unsigned long offDelayMs = (unsigned long)runtime.auto_off_delay_s * 1000UL;
    bool motionTimeoutElapsed = runtime.motion_aktiv
                             && runtime.letzte_motion_ms > 0
                             && (nowMs - runtime.letzte_motion_ms) >= offDelayMs;

    if (!motionTimeoutElapsed) {
        return;  // Nachlauf laeuft noch
    }

    // Nachlauf abgelaufen → Motion-Off
    runtime.motion_aktiv = false;                   // Praesenz endet
    runtime.blocked_by_lux = false;                 // Lux-Sperre aufheben
    runtime.pending_auto_on_decision = false;       // Ausstehende Entscheidung loeschen
    runtime.state_report_offen = true;              // STATE-Update beim Master anfordern
    runtime.pending_motion_event_state = 2U;        // 2 = Motion-OFF Event steht aus

    // Wenn Relais AN und von Auto-Light gesteuert → ausschalten
    if (runtime.relay_1 && runtime.relay_auto_owned) {
        setRelay(false, "auto_off");
        sendRelayEvent(SH_TRIGGER_AUTO_OFF_TIMER);
        runtime.relay_auto_owned = false;           // Relais nicht mehr unter Auto-Kontrolle
    }
    if (runtime.relay_1 && runtime.manual_follow_motion_active && runtime.manual_follow_motion_seen) {
        turnOffManualFollowRelay("manual_follow_motion_off");
    }
}

#if defined(NET_ERL_HAS_BUTTON)
// =============================================================================
// processBtn – Taster entprellen und Short-Press/Long-Press unterscheiden.
//
// WAS: Liest den Taster, entprellt ihn (NET_ERL_BUTTON_DEBOUNCE_MS), und unterscheidet:
//      - Short-Press (< SETUP_BUTTON_HOLD_MS): Relais toggeln
//      - Long-Press  (>= SETUP_BUTTON_HOLD_MS): Ignorieren (wird vom
//        Provisioning-Controller fuer Setup-Mode verarbeitet)
//
// WARUM: Manuelles Relais-Schalten ohne Master/ESP-NOW.
//
// PARAM nowMs: Aktuelle Zeit in Millisekunden (millis()).
// =============================================================================
void processBtn(unsigned long nowMs) {
    bool rawState = netErlDeviceReadButton();

    // Flanken-Erkennung: Wechsel des Raw-Signal speichern
    if (rawState != runtime.button_raw_active) {
        runtime.button_raw_active = rawState;
        runtime.button_changed_at_ms = nowMs;
    }

    // Entprellung: Ignoriere Wechsel die kuerzer als DEBOUNCE_MS sind
    unsigned long timeSinceChange = nowMs - runtime.button_changed_at_ms;
    if (timeSinceChange < NET_ERL_BUTTON_DEBOUNCE_MS) {
        return;
    }

    // Kein Zustandswechsel im stabilen Signal → nichts tun
    if (rawState == runtime.button_stable_active) {
        return;
    }
    runtime.button_stable_active = rawState;

    // --- TASTE GEDRUECKT (fallende/flankende Flanke je nach Logik) ---
    if (rawState) {
        runtime.button_pressed_at_ms = nowMs;
        return;  // Warte auf Loslassen
    }

    // --- TASTE LOSGELASSEN → Haltezeit auswerten ---
    unsigned long holdDurationMs = (runtime.button_pressed_at_ms > 0)
                                 ? (nowMs - runtime.button_pressed_at_ms)
                                 : 0;
    runtime.button_pressed_at_ms = 0;

    // Long-Press → Setup-Mode (wird vom Provisioning-Controller behandelt)
    if (holdDurationMs >= SETUP_BUTTON_HOLD_MS) {
        return;
    }

    // Short-Press → Relais toggeln. Button-AN folgt danach derselben Motion-
    // Folgelogik wie Master-AN; Button-AUS ist ein echtes AUS und bricht alles ab.
    bool newRelayState = !runtime.relay_1;
    setRelay(newRelayState, "button");
    if (newRelayState) {
        startManualFollowMotion(nowMs, "button");
    } else {
        runtime.relay_auto_owned = false;
        runtime.pending_auto_on_decision = false;
        runtime.blocked_by_lux = false;
        cancelManualFollowMotion("button_off");
        holdExplicitOffUntilNextMotionRisingEdge(nowMs);
    }
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

    if (!runtime.provisioning_bereit) { logMsg("WARN", "Prov init fail"); return; }

    setReportInt(nodeProvisioning.sanitizeStatusSendInterval(runtime.report_interval_s));
    runtime.stored_sensor_send_interval_s = nodeProvisioning.sanitizeSensorSendInterval(runtime.stored_sensor_send_interval_s);
    logMsg("INFO", "%s v%s (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logMsg("INFO", "%s %s %s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);

    if (!nodeProvisioning.hasStoredMasterMac()) { nodeProvisioning.enterSetupMode(); return; }
    initFunk(); sendHello();
}

// =============================================================================
// ARDUINO – loop()
//
// ABLAUF JEDER ITERATION:
//   1. Watchdog zuruecksetzen
//   2. Provisioning-Controller updaten (Setup-Mode, AP-Verwaltung)
//   3. Wenn Setup-Mode: nur warten, keine Funk-Aktivitaet
//   4. ESP-NOW initialisieren (falls noch nicht geschehen)
//   5. Geraete-spezifische Tasks:
//      - Praesenzsensor pollen (Auto-Light)
//      - Taster entprellen (manuelles Schalten)
//      - Sensoren lesen (Temperatur, Lux, Gas, etc.)
//      - Ausstehende CMDs verarbeiten (nur Hall mit ISR-Queue)
//   6. Ausstehende Motion-Events senden
//   7. Periodische Protokoll-Nachrichten:
//      - HELLO (wenn Master noch nicht bekannt)
//      - HEARTBEAT (wenn Master bekannt)
//      - STATE (wenn angefordert oder Intervall abgelaufen)
//   8. Debug-Snapshot-Log (wenn aktiv)
//   9. Warten (LOOP_INTERVAL_MS)
// =============================================================================

void loop() {
    esp_task_wdt_reset();
    nodeProvisioning.update();

    // Wenn Setup-Mode: nur Provisioning updaten, keine Funk-Aktivitaet
    if (!runtime.provisioning_bereit || runtime.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    // ESP-NOW initialisieren falls noch nicht geschehen
    if (!runtime.funk_bereit) {
        initFunk();
    }

    unsigned long nowMs = millis();

    // Geraete-spezifische Tasks
    pollPresence(nowMs);
    processBtn(nowMs);
    netErlDevicePollSensors(nowMs);
#if NET_ERL_USE_ISR_CMD_QUEUE
    verarbeiteAusstehende();
#endif

    // Ausstehendes Motion-Event senden
    if (runtime.pending_motion_event_state != 0U && runtime.master_bekannt && runtime.master_mac_gueltig) {
        if (sendMotionEvent(runtime.pending_motion_event_state == 1U)) {
            runtime.pending_motion_event_state = 0U;
        }
    }

    // Periodische Protokoll-Nachrichten
    if (!runtime.master_bekannt && (nowMs - runtime.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendHello();
    }
    if (runtime.master_bekannt && (nowMs - runtime.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
    }

    // STATE senden wenn: Master bekannt UND (einer der drei Ausloeser):
    //   1. state_report_offen = Master hat STATE angefordert oder Event steht an
    //   2. letzter_state_ms == 0 = Noch nie STATE gesendet (nach Boot)
    //   3. Report-Intervall abgelaufen
    bool masterReady = runtime.master_bekannt && runtime.master_mac_gueltig;
    bool stateRequested = runtime.state_report_offen;
    bool neverSentBefore = (runtime.letzter_state_ms == 0);
    bool reportIntervalElapsed = (runtime.state_interval_ms > 0)
                              && ((nowMs - runtime.letzter_state_ms) >= runtime.state_interval_ms);

    bool shouldSendState = masterReady && (stateRequested || neverSentBefore || reportIntervalElapsed);
    if (shouldSendState) {
        sendState();
    }

    // Snapshot-Log (Debug)
    if (intervalElapsed(runtime.letztes_snap_log_ms, nowMs, SNAPSHOT_LOG_INTERVAL_MS) && DEBUG_LOKAL_AKTIV) {
        netErlDeviceLogSnapshot();
        runtime.letztes_snap_log_ms = nowMs;
    }

    delay(LOOP_INTERVAL_MS);
}
