/**
 * @file main.cpp
 * @brief NET-ERL Basistyp: Netz-Relais mit ESP-NOW (1-Kanal)
 *
 * @details ESP-NOW-basierte Relais-Steuerung. Sendet HELLO/HEARTBEAT/STATE,
 *          empfaengt CMD (set_relay, state_request) und CFG (report_interval).
 *          ShNodeProvisioning fuer Master-Bindung, Web-Provisioning und NVS.
 *
 * Protokoll:
 *   - Senden:   HELLO, HEARTBEAT, STATE, EVENT (RELAY_CHANGED), ACK
 *   - Empfangen: HELLO_ACK, CMD, CFG
 *
 * @author DevOpsOfChaos
 * @date   2026-05-14
 *
 * @note Alle Funktionen im Rest der Datei sind bereits mit @brief/@param/@return
 *       im Doxygen-Stil dokumentiert (deutsch).
 */

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

// Fallback fuer aeltere ESP32 Arduino-Cores ohne Version-Header
#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

// =============================================================================
// KONSTANTEN – Geraete-Identifikation, Timing und Broadcast-Adresse
// =============================================================================

// Debug-Ausgaben nur wenn NET_ERL_DEBUG_ENABLED=1 UND globales DEBUG_AKTIV=1
constexpr bool DEBUG_LOKAL_AKTIV = (NET_ERL_DEBUG_ENABLED != 0) && DEBUG_AKTIV;
// Kurzbezeichnung des Geraetetyps fuer Log-Ausgaben
constexpr char DATEI_GERAET[] = "NET-ERL";
// Firmware-Version als String
constexpr char DATEI_VERSION[] = "0.1.0";
// ESP-NOW Broadcast-Adresse (MAC  FF:FF:FF:FF:FF:FF)
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Geraete-Identitaet (aus DeviceConfig.h)
constexpr char DEVICE_ID[] = NET_ERL_DEVICE_ID;
constexpr char DEVICE_NAME[] = NET_ERL_DEVICE_NAME;
constexpr char FW_VARIANT[] = NET_ERL_FW_VARIANT;
// Faehigkeiten als Bitmaske (hier: Bit 0 = Relais)
constexpr uint16_t DEVICE_CAPS = (uint16_t)NET_ERL_DEVICE_CAPS;
// Steuerungsmodus: SH_CONTROL_MODE_RELAY
constexpr uint8_t DEVICE_CONTROL_MODE = NET_ERL_DEVICE_CONTROL_MODE;
// Konfigurationsprofil: SH_PROFILE_NONE (keine Sonderlogik)
constexpr uint8_t DEVICE_CONFIG_PROFILE = NET_ERL_DEVICE_CONFIG_PROFILE;
// Report-Modus: SH_REPORTING_HYBRID (regelmaessig + Event-basiert)
constexpr uint8_t DEVICE_REPORTING_MODE = NET_ERL_DEVICE_REPORTING_MODE;
// Aktuelle Version des Meta-Daten-Schemas
constexpr uint8_t DEVICE_META_SCHEMA_VERSION = SH_META_SCHEMA_VERSION_CURRENT;

// ESP-NOW WLAN-Kanal (Default: 6)
constexpr int WLAN_KANAL = NET_ERL_WLAN_CHANNEL;
// Wiederholungsintervall fuer HELLO-Nachrichten (Default: 5s)
constexpr unsigned long HELLO_RETRY_INTERVAL_MS = NET_ERL_HELLO_RETRY_INTERVAL_MS;
// Heartbeat-Intervall (Default: 20s)
constexpr unsigned long HEARTBEAT_INTERVAL_MS = NET_ERL_HEARTBEAT_INTERVAL_MS;
// Loop-Intervall (Default: 20ms) – Polling-Zyklus
constexpr unsigned long LOOP_INTERVAL_MS = NET_ERL_LOOP_INTERVAL_MS;
// Minimales Report-Intervall in Sekunden (Default: 5s)
constexpr uint16_t MIN_REPORT_INTERVAL_S = NET_ERL_MIN_REPORT_INTERVAL_S;
// Maximales Report-Intervall in Sekunden (Default: 600s = 10min)
constexpr uint16_t MAX_REPORT_INTERVAL_S = NET_ERL_MAX_REPORT_INTERVAL_S;
// Boot-Zaehler (wird bei jedem Neustart inkrementiert)
constexpr uint32_t BOOT_COUNTER = NET_ERL_BOOT_COUNTER;
// Standard-Report-Intervall aus STATE_INTERVAL_MS umgerechnet (Default: 10s)
constexpr uint32_t DEFAULT_REPORT_INTERVAL_S = NET_ERL_STATE_INTERVAL_MS / 1000UL;
// Sensor-Sende-Intervall (identisch mit Report-Intervall bei NET-ERL)
constexpr uint32_t DEFAULT_SENSOR_SEND_INTERVAL_S = DEFAULT_REPORT_INTERVAL_S;

// Puffergroesse fuer Setup-AP-SSID (DEVICE_ID muss hineinpassen)
constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
static_assert(
    sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "NET_ERL_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");

// =============================================================================
// NODE STATUS – Zentraler Geraetezustand (alle Status-Flags und Timer)
// =============================================================================
struct NodeState {
    bool provisioning_bereit;           // true = NodeProvisioning erfolgreich initialisiert
    bool setup_mode;                    // true = Geraet ist im Web-Setup-Modus
    bool setup_ap_aktiv;                // true = Setup-AccessPoint laeuft
    bool restart_pending;               // true = Neustart wurde angefordert
    bool master_bekannt;                // true = HELLO_ACK vom Master empfangen
    bool master_mac_gueltig;            // true = Master-MAC-Adresse ist provisioniert
    bool state_report_offen;            // true = STATE muss gesendet werden (dirty-Flag)
    bool funk_bereit;                   // true = ESP-NOW erfolgreich initialisiert
    bool relay_1;                       // Aktueller Relais-Zustand (true = EIN)
    bool fault;                         // Fehlerflag fuer STATE-Report
    unsigned long letztes_hello_ms;     // millis()-Zeitstempel des letzten HELLO
    unsigned long letzter_heartbeat_ms; // millis()-Zeitstempel des letzten HEARTBEAT
    unsigned long letzter_state_ms;     // millis()-Zeitstempel des letzten STATE
    unsigned long restart_requested_at_ms; // millis()-Zeitstempel der Restart-Anforderung
    unsigned long state_interval_ms;    // Aktuelles STATE-Intervall in ms (aus report_interval_s)
    uint32_t report_interval_s;         // Aktuelles Report-Intervall in Sekunden
    uint32_t stored_sensor_send_interval_s; // Gespeichertes Sensor-Sende-Intervall (nicht genutzt)
    uint8_t master_mac[6];              // Provisionierte MAC-Adresse des Masters
    uint8_t naechste_seq;               // Naechste ESP-NOW-Sequenznummer (ueberlaeuft)
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE]; // SSID des Setup-AccessPoints
};

// Instanz des zentralen Geraetezustands (null-initialisiert)
NodeState nodeStatus = {};

// =============================================================================
// HILFSFUNKTIONEN – Logging, String-Kopien, MAC-Prüfungen
// =============================================================================

// logf – Formatiertes Logging (nur bei aktiviertem Debug)
//   Gibt formatierte Meldungen auf Serial aus mit Prefix [level].
//   Parameter: level(const char*) – Log-Level (INFO, WARN, ERROR)
//              format(const char*) – printf-Format-String
//              ... (variadic) – Format-Argumente
//   Rückgabe: keine
void logf(const char* level, const char* format, ...) {
    // Prueft ob Debug-Ausgaben global und lokal aktiviert sind
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

// provisioningLog – Logging-Callback fuer das Provisioning-Framework
//   Wird intern von ShNodeProvisioning fuer Statusmeldungen aufgerufen.
//   Nur aktiv wenn DEBUG_LOKAL_AKTIV=true und gueltige Parameter.
//   Parameter: level(const char*) – Log-Level
//              message(const char*) – Nachrichtentext
//   Rückgabe: keine
void provisioningLog(const char* level, const char* message) {
    // Prueft ob Debug aktiv und Parameter nicht null
    if (!DEBUG_LOKAL_AKTIV || level == nullptr || message == nullptr) return;

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// copyText – Sicheres Kopieren eines null-terminierten Strings
//   Stellt sicher dass das Ziel immer null-terminiert ist.
//   Verhindert Pufferueberlaeufe durch strncpy mit targetSize-1.
//   Parameter: target(char*) – Zielpuffer
//              targetSize(size_t) – Groesse des Zielpuffers (max. Zeichen + 1)
//              source(const char*) – Quell-String (darf nullptr sein)
//   Rückgabe: keine
void copyText(char* target, size_t targetSize, const char* source) {
    // Prueft ob Zielpuffer gueltig
    if (!target || targetSize == 0U) return;
    // Quelle nullptr: leeren String setzen
    if (!source) {
        target[0] = '\0';
        return;
    }

    strncpy(target, source, targetSize - 1U);
    target[targetSize - 1U] = '\0';
}

// istBroadcastMac – Prueft ob eine MAC-Adresse die Broadcast-Adresse ist
//   Parameter: mac(const uint8_t[6]) – Zu pruefende MAC-Adresse
//   Rückgabe: true = Broadcast-MAC (FF:FF:FF:FF:FF:FF)
bool istBroadcastMac(const uint8_t* mac) {
    return mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0;
}

// senderIstProvisionierterMaster – Prueft ob der Sender der provisionierte Master ist
//   Vergleicht die Sender-MAC mit der gespeicherten Master-MAC.
//   Nur gueltig wenn master_mac_gueltig=true.
//   Parameter: senderMac(const uint8_t[6]) – MAC des Senders
//   Rückgabe: true = Sender ist der provisionierte Master
bool senderIstProvisionierterMaster(const uint8_t* senderMac) {
    return nodeStatus.master_mac_gueltig &&
           senderMac != nullptr &&
           memcmp(senderMac, nodeStatus.master_mac, sizeof(nodeStatus.master_mac)) == 0;
}

// holeHelloZielMac – Bestimmt die Ziel-MAC fuer HELLO-Nachrichten
//   Wenn Master-MAC bekannt: direkt an Master senden.
//   Wenn nicht bekannt: Broadcast (zur Master-Suche).
//   Parameter: keine
//   Rückgabe: const uint8_t* – Ziel-MAC (Master oder Broadcast)
const uint8_t* holeHelloZielMac() {
    return nodeStatus.master_mac_gueltig ? nodeStatus.master_mac : BROADCAST_MAC;
}

// wendeReportIntervalAn – Setzt das Report-Intervall und rechnet es in ms um
//   Aktualisiert report_interval_s und state_interval_ms.
//   Parameter: wertS(uint32_t) – Neues Report-Intervall in Sekunden
//   Rückgabe: keine
void wendeReportIntervalAn(uint32_t wertS) {
    nodeStatus.report_interval_s = wertS;
    nodeStatus.state_interval_ms = (unsigned long)nodeStatus.report_interval_s * 1000UL;
}

// =============================================================================
// PROVISIONING-HANDLER – Web-Konfiguration (NetErlProvisioningHandler)
// =============================================================================
// Kapselt das Web-Provisioning-Interface. Stellt Ueberschriften und
// Intro-Texte fuer das Setup-Portal bereit. Keine zusaetzlichen
// geraetespezifischen Setup-Felder (keine Sensoren, keine Schwellwerte).
// =============================================================================
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

// Globale Instanzen des Provisioning-Controllers und des Device-Handlers
SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetErlProvisioningHandler netErlProvisioningHandler;

// speichereReportIntervalMitRollback – Wendet ein neues Sende-Intervall an und persistiert es
//   Sichert zuerst den aktuellen Zustand (Basis-Snapshot). Bei Fehlschlag
//   des Speicherns wird der alte Zustand wiederhergestellt (Rollback).
//   Parameter: valueS(uint32_t) – Neues Intervall in Sekunden
//   Rückgabe: true = erfolgreich gespeichert und angewendet
bool speichereReportIntervalMitRollback(uint32_t valueS) {
    // Prueft ob das Intervall im gueltigen Bereich liegt
    if (!nodeProvisioning.isSendIntervalValid(valueS)) return false;

    // Snapshot erstellen fuer moegliches Rollback
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    nodeProvisioning.captureBasisSnapshot(basisSnapshot);

    // Neues Intervall anwenden und dirty-Flag setzen
    wendeReportIntervalAn(valueS);
    nodeStatus.state_report_offen = true;

    // Persistieren – bei Erfolg fertig
    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }

    // Rollback: alte Werte wiederherstellen
    nodeProvisioning.restoreBasisSnapshot(basisSnapshot);
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    return false;
}

// =============================================================================
// HARDWARE – GPIO-Relais-Ausgang setzen
// =============================================================================

// setzeRelayAusgang – Setzt den physikalischen Relais-Pin (digitalWrite)
//   Beruecksichtigt active-HIGH/LOW-Konfiguration fuer den Pegel.
//   Steuert optional eine Status-LED synchron mit dem Relais.
//   Parameter: an(bool) – true = Relais EIN, false = Relais AUS
//   Rückgabe: keine
void setzeRelayAusgang(bool an) {
    // Pegel je nach active-HIGH/LOW-Konfiguration setzen
    digitalWrite(PIN_RELAY_1, (an == RELAY_1_ACTIVE_HIGH) ? HIGH : LOW);

#if PIN_STATUS_LED >= 0
    // Optionale Status-LED: HIGH wenn Relais EIN, LOW wenn AUS
    digitalWrite(PIN_STATUS_LED, an ? HIGH : LOW);
#endif

    logf("INFO", "GPIO%d relay_1 -> %s", PIN_RELAY_1, an ? "HIGH" : "LOW");
}

// =============================================================================
// KOMMUNIKATION – ESP-NOW Sende- und Empfangs-Infrastruktur
// =============================================================================

// stellePeerSicher – Registriert eine MAC-Adresse als ESP-NOW-Peer
//   Prueft ob Peer bereits existiert. Validiert MAC und Funk-Status.
//   Legt Kanal und Verschlüsselung (aus) fest.
//   Parameter: mac(const uint8_t[6]) – MAC-Adresse des Peers
//   Rückgabe: true = Peer existiert oder wurde erfolgreich angelegt
bool stellePeerSicher(const uint8_t* mac) {
    // Prueft ob Funk bereit und MAC nicht null
    if (!nodeStatus.funk_bereit || mac == nullptr) return false;
    // Broadcast und ungueltige MACs ablehnen
    if (!istBroadcastMac(mac) && !SmartHome::isValidMac(mac)) return false;
    // Peer existiert bereits – nichts zu tun
    if (esp_now_is_peer_exist(mac)) return true;

    // Neuen Peer anlegen mit aktuellem WLAN-Kanal, ohne Verschluesselung
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

// sendePaketMitOptionen – Zentrale ESP-NOW-Sendefunktion mit Flags und Sequenz
//   Baut ein ESP-NOW-Paket aus Header + Payload, fuegt CRC hinzu und sendet.
//   Optionale Rueckgabe der verwendeten Sequenznummer.
//   Parameter: zielMac(const uint8_t[6]) – Ziel-MAC-Adresse
//              msgType(uint8_t) – Nachrichtentyp (SH_MSG_HELLO, SH_MSG_STATE, ...)
//              payload(const void*) – Nutzdaten (darf nullptr sein wenn len=0)
//              payloadLen(size_t) – Laenge der Nutzdaten in Bytes
//              label(const char*) – Bezeichnung fuer Debug-Ausgabe (z.B. "HELLO")
//              flags(uint8_t) – ESP-NOW-Flags (z.B. SH_FLAG_ACK_REQUEST)
//              verwendeteSeq(uint8_t*) – Ausgabe: verwendete Sequenznummer (darf nullptr sein)
//   Rückgabe: true = erfolgreich gesendet
bool sendePaketMitOptionen(
    const uint8_t* zielMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label,
    uint8_t flags,
    uint8_t* verwendeteSeq)
{
    // Prueft ob Funk bereit und Payload nicht zu gross
    if (!nodeStatus.funk_bereit || zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    // Stellt sicher dass der Peer registriert ist
    if (!stellePeerSicher(zielMac)) return false;

    // Paket-Puffer und Header initialisieren
    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t seq = nodeStatus.naechste_seq++;
    SmartHome::fillHeader(header, msgType, seq, flags, (uint16_t)payloadLen);

    // Payload in den Puffer kopieren (hinter dem Header)
    uint8_t* payloadBuffer = packet + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    // CRC berechnen, Header finalisieren, senden
    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(packet, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, packet, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label, (int)err);
        return false;
    }

    // Optionale Rueckgabe der verwendeten Sequenznummer
    if (verwendeteSeq != nullptr) {
        *verwendeteSeq = seq;
    }

    return true;
}

// sendePaket – Vereinfachte Sendefunktion ohne Flags/Rueckgabe
//   Wrapper um sendePaketMitOptionen mit flags=0 und ohne Seq-Ausgabe.
//   Parameter: zielMac, msgType, payload, payloadLen, label
//   Rückgabe: true = erfolgreich gesendet
bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    return sendePaketMitOptionen(zielMac, msgType, payload, payloadLen, label, 0U, nullptr);
}

// sendeAck – Sendet eine ACK-Bestaetigung (ACK-Message)
//   Parameter: zielMac(const uint8_t[6]) – Ziel-MAC
//              ackSeq(uint8_t) – Sequenz der zu bestaetigenden Nachricht
//              ackMsgType(uint8_t) – Nachrichtentyp den wir bestaetigen
//              status(uint8_t) – ACK-Status (SH_ACK_OK, SH_ACK_REJECTED, ...)
//   Rückgabe: true = erfolgreich gesendet
bool sendeAck(const uint8_t* zielMac, uint8_t ackSeq, uint8_t ackMsgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = ackSeq;
    payload.ack_msg_type = ackMsgType;
    payload.status = status;
    return sendePaket(zielMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN – HELLO, HEARTBEAT, STATE, EVENT
// =============================================================================

// sendeHello – Sendet eine HELLO-Nachricht zur Master-Anmeldung
//   Enthaelt alle Meta-Daten: device_id, device_name, device_class, caps,
//   power_type, fw_version, boot_counter, schema_version, control_mode,
//   config_profile, reporting_mode, sensor_mask, input_mask.
//   Ziel: Master-MAC (wenn bekannt) oder Broadcast (Master-Suche).
//   Parameter: keine
//   Rückgabe: true = erfolgreich gesendet
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

// sendeHeartbeat – Sendet einen HEARTBEAT an den Master
//   Informiert den Master dass das Geraet noch aktiv ist.
//   Enthaelt node_id und aktuelle Uptime in Sekunden.
//   Nur moeglich wenn Master-MAC bekannt.
//   Parameter: keine
//   Rückgabe: true = erfolgreich gesendet
bool sendeHeartbeat() {
    // Prueft ob Master-MAC gueltig (ohne Master kein Heartbeat moeglich)
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

// sendeState – Sendet den aktuellen Geraetezustand (STATE) an den Master
//   Enthaelt node_id, relay_1 (0/1), fault (0/1).
//   Loescht nach erfolgreichem Senden das dirty-Flag state_report_offen.
//   Nur moeglich wenn Master-MAC bekannt.
//   Parameter: keine
//   Rückgabe: true = erfolgreich gesendet
bool sendeState() {
    // Prueft ob Master-MAC gueltig (ohne Master kein State moeglich)
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::StateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.relay_1 = nodeStatus.relay_1 ? 1U : 0U;
    payload.fault = nodeStatus.fault ? 1U : 0U;

    if (!sendePaket(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    // Dirty-Flag zuruecksetzen: kein offener State-Report mehr
    nodeStatus.state_report_offen = false;
    nodeStatus.letzter_state_ms = millis();
    return true;
}

// sendeRelayEvent – Sendet ein EVENT bei Relais-Zustandsaenderung
//   event_type=RELAY_CHANGED, trigger=ausloesende Ursache, param1=neuer Zustand
//   Parameter: trigger(uint8_t) – Ausloeser (SH_TRIGGER_MASTER_CMD oder anderer)
//   Rückgabe: true = erfolgreich gesendet
bool sendeRelayEvent(uint8_t trigger) {
    // Prueft ob Master-MAC gueltig
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = SH_EVENT_RELAY_CHANGED;
    payload.trigger = trigger;
    payload.param1 = nodeStatus.relay_1 ? 1U : 0U;
    payload.param2 = 0U;

    return sendePaket(nodeStatus.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT_RELAY_CHANGED");
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG – Eingehende Nachrichten (HELLO_ACK, CMD, CFG)
// =============================================================================

// verarbeiteHelloAck – Verarbeitet eine HELLO_ACK-Antwort vom Master
//   Prueft ACK-Status und Master-Identitaet.
//   Bei positivem ACK vom provisionierten Master: master_bekannt=true setzen,
//   state_report_offen triggern, Peer aktualisieren.
//   Parameter: senderMac(const uint8_t[6]) – MAC des Senders (Master)
//              payload(const SmartHome::HelloAckPayload&) – ACK-Daten
//   Rückgabe: keine
void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    // Prueft ob ACK-Status OK ist (nicht abgelehnt)
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    // Prueft ob Master-MAC ueberhaupt provisioniert wurde
    if (!nodeStatus.master_mac_gueltig) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return;
    }

    // Prueft ob Sender der provisionierte Master ist (Sicherheit)
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    // Master als bekannt markieren und State triggern
    nodeStatus.master_bekannt = true;
    nodeStatus.state_report_offen = true;
    stellePeerSicher(nodeStatus.master_mac);
    logf("INFO", "HELLO_ACK empfangen");
}

// uebernehmeCfg – Wendet einen Konfigurationswert an (momentan nur report_interval)
//   Parameter: payload(const SmartHome::CfgPayload&) – Konfig-Value (param_id + value)
//   Rückgabe: true = erfolgreich uebernommen
bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    // Prueft ob es sich um das Report-Intervall handelt (einziger unterstuetzter CFG-Wert)
    if (payload.param_id != SH_CFG_REPORT_INTERVAL_S) return false;

    const bool ok = speichereReportIntervalMitRollback(payload.value);
    if (!ok) {
        logf("WARN", "report_interval_s konnte nicht uebernommen werden");
    }
    return ok;
}

// verarbeiteCfg – Verarbeitet eine eingehende CFG-Nachricht
//   Validiert Master-Identitaet, wendet den Konfigurationswert an.
//   Sendet bei Bedarf ACK zurueck (wenn SH_FLAG_ACK_REQUEST gesetzt).
//   Parameter: senderMac(const uint8_t[6]) – MAC des Senders
//              header(const SmartHome::MsgHeader&) – Nachrichtenkopf
//              payload(const SmartHome::CfgPayload&) – Konfig-Daten
//   Rückgabe: keine
void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    // Prueft ob Sender der provisionierte Master ist
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CFG ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    const bool ok = uebernehmeCfg(payload);
    // Ggf. ACK senden wenn vom Master angefordert
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

// verarbeiteCmd – Verarbeitet eine eingehende CMD-Nachricht
//   Validiert Master-Identitaet und Kommando-Typ.
//   Unterstuetzt:
//     SH_CMD_STATE_REQUEST – Loest STATE-Sendung aus
//     SH_CMD_SET_RELAY – Schaltet das Relais ein/aus (param1=index, param2=zustand)
//   Parameter: senderMac(const uint8_t[6]) – MAC des Senders
//              header(const SmartHome::MsgHeader&) – Nachrichtenkopf
//              payload(const SmartHome::CmdPayload&) – Kommando-Daten
//   Rückgabe: keine
void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    // Prueft ob Sender der provisionierte Master ist
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    // STATE_REQUEST – kein Payload, nur dirty-Flag setzen
    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        nodeStatus.state_report_offen = true;
        return;
    }

    // SET_RELAY – Relais schalten (param1=index, param2=zustand)
    if (payload.cmd_type == SH_CMD_SET_RELAY) {
        const bool gueltigerIndex = (payload.param1 == 0U);
        const bool neuerZustand = (payload.param2 != 0U);

        // Prueft ob der Relais-Index gueltig ist (nur Index 0 existiert)
        if (!gueltigerIndex) {
            if (header.flags & SH_FLAG_ACK_REQUEST) {
                sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
            }
            logf("WARN", "SET_RELAY verworfen: relay_index=%u", (unsigned)payload.param1);
            return;
        }

        // Relais-Zustand aktualisieren: GPIO setzen, Status speichern, Event senden
        nodeStatus.relay_1 = neuerZustand;
        setzeRelayAusgang(nodeStatus.relay_1);
        nodeStatus.state_report_offen = true;
        sendeRelayEvent(SH_TRIGGER_MASTER_CMD);

        // Ggf. ACK senden bei Erfolg
        if (header.flags & SH_FLAG_ACK_REQUEST) {
            sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_OK);
        }

        return;
    }

    // Unbekanntes Kommando: ACK mit REJECTED senden
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, SH_ACK_REJECTED);
    }
}

// verarbeiteEspNowPaket – Zentrale ESP-NOW-Empfangsverarbeitung
//   Validiert das Paket (Gueltigkeit, CRC) und leitet an den passenden
//   Nachrichtenhandler weiter (HELLO_ACK, CMD, CFG). Switch-Case nach msg_type.
//   Parameter: senderMac(const uint8_t[6]) – MAC des Senders
//              data(const uint8_t*) – Rohdaten des ESP-NOW-Pakets
//              len(int) – Laenge der Rohdaten
//   Rückgabe: keine
void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* data, int len) {
    // Prueft ob Paket-Daten gueltig und gross genug fuer Header
    if (!senderMac || !data || len < (int)sizeof(SmartHome::MsgHeader)) return;
    // Prueft CRC-Checksumme des Pakets
    if (!SmartHome::hasValidPacketCrc(data, (size_t)len)) return;

    const SmartHome::MsgHeader* header = reinterpret_cast<const SmartHome::MsgHeader*>(data);
    const uint8_t* payload = data + SH_HEADER_SIZE;

    // Anhand msg_type an den richtigen Handler weiterleiten
    switch (header->msg_type) {
        case SH_MSG_HELLO_ACK:
            // Prueft ob Payload-Laenge korrekt ist
            if (header->payload_len == sizeof(SmartHome::HelloAckPayload)) {
                verarbeiteHelloAck(senderMac, *reinterpret_cast<const SmartHome::HelloAckPayload*>(payload));
            }
            break;

        case SH_MSG_CMD:
            // Prueft ob Payload-Laenge korrekt ist
            if (header->payload_len == sizeof(SmartHome::CmdPayload)) {
                verarbeiteCmd(senderMac, *header, *reinterpret_cast<const SmartHome::CmdPayload*>(payload));
            }
            break;

        case SH_MSG_CFG:
            // Prueft ob Payload-Laenge korrekt ist
            if (header->payload_len == sizeof(SmartHome::CfgPayload)) {
                verarbeiteCfg(senderMac, *header, *reinterpret_cast<const SmartHome::CfgPayload*>(payload));
            }
            break;

        default:
            // Unbekannter Nachrichtentyp – ignorieren
            break;
    }
}

// =============================================================================
// ESP-NOW-CALLBACKS – Ereignisbehandlung fuer Senden/Empfangen
// =============================================================================

// onEspNowReceive – Wird bei eingehendem ESP-NOW-Paket aufgerufen
//   Zwei Versionen: ESP Arduino Core >= 3 (recv_info_t) und < 3 (senderMac)
//   Ruft verarbeiteEspNowPaket auf.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    // ESP32 Core v3+ API: MAC in info->src_addr
    if (!info) return;
    verarbeiteEspNowPaket(info->src_addr, data, len);
}
#else
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* data, int len) {
    // ESP32 Core v2 API: MAC direkt als Parameter
    verarbeiteEspNowPaket(senderMac, data, len);
}
#endif

// onEspNowSend – Callback nach ESP-NOW-Sendevorgang
//   Wird aufgerufen wenn ein ESP-NOW-Paket gesendet (oder verworfen) wurde.
//   Loggt eine Warnung bei Sendefehler (ist informativ, kein kritischer Fehler).
void onEspNowSend(const wifi_tx_info_t* /*tx_info*/, esp_now_send_status_t status) {
    // Prueft ob das Senden fehlgeschlagen ist
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

// =============================================================================
// INITIALISIERUNG – Funk und Geraete-Start
// =============================================================================

// initialisiereFunk – Initialisiert ESP-NOW-Funkmodul
//   Setzt WLAN auf Station-Modus, deaktiviert Sleep, stellt Kanal ein,
//   registriert ESP-NOW-Callbacks. Legt Broadcast und Master als Peer an.
//   Wird nur ausgefuehrt wenn nicht bereits im Setup-Modus.
//   Parameter: keine
//   Rückgabe: keine
void initialisiereFunk() {
    // Prueft ob Funk bereits initialisiert oder Setup-Modus aktiv
    if (nodeStatus.funk_bereit || nodeStatus.setup_mode) return;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);

    // WLAN-Kanal setzen (wichtig: Master und alle Nodes gleicher Kanal)
    const esp_err_t kanalErr = esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (kanalErr != ESP_OK) {
        logf("WARN", "WLAN-Kanal %d konnte nicht gesetzt werden (err=%d)", WLAN_KANAL, (int)kanalErr);
    }

    // ESP-NOW initialisieren
    if (esp_now_init() != ESP_OK) {
        logf("WARN", "ESP-NOW Initialisierung fehlgeschlagen");
        return;
    }

    // Callbacks registrieren
    esp_now_register_send_cb(onEspNowSend);
    esp_now_register_recv_cb(onEspNowReceive);
    nodeStatus.funk_bereit = true;

    // Broadcast-Peer anlegen (immer noetig)
    stellePeerSicher(BROADCAST_MAC);
    // Master-Peer anlegen (nur wenn bereits bekannt)
    if (nodeStatus.master_mac_gueltig) {
        stellePeerSicher(nodeStatus.master_mac);
    }
}

// setup – Arduino-Hauptinitialisierung (einmalig beim Start)
//   Initialisiert Serial (bei Debug), GPIOs, NodeProvisioning fuer
//   Master-Bindung und Setup-AP. Setzt alle Status-Flags auf Default.
//   Wenn Master persistiert: Funk starten und HELLO senden.
//   Wenn kein Master: in den Setup-Modus gehen (Web-AP).
//   Parameter: keine  |  Rückgabe: keine
void setup() {
    // Serial initialisieren (nur wenn Debug aktiv)
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    // Alle Status-Flags null-initialisieren, dann Defaults setzen
    nodeStatus = {};
    nodeStatus.report_interval_s = DEFAULT_REPORT_INTERVAL_S;
    nodeStatus.stored_sensor_send_interval_s = DEFAULT_SENSOR_SEND_INTERVAL_S;
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    nodeStatus.state_report_offen = true;
    nodeStatus.relay_1 = false;
    nodeStatus.fault = false;

    // GPIOs initialisieren: Relais aus, optionale Status-LED aus
    pinMode(PIN_RELAY_1, OUTPUT);
    setzeRelayAusgang(false);

#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif

    // NodeProvisioning konfigurieren und starten
    SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig =
        SmartHome::NetErlProvisioning::makeConfig(
            DEVICE_ID,
            DEFAULT_REPORT_INTERVAL_S,
            DEFAULT_SENSOR_SEND_INTERVAL_S,
            MIN_REPORT_INTERVAL_S,
            MAX_REPORT_INTERVAL_S);
    provisioningConfig.setupButtonPin = SETUP_BUTTON_PIN;
    provisioningConfig.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    provisioningConfig.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS;
    provisioningConfig.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    provisioningConfig.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0;
    provisioningConfig.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    // Provisioning-Controller initialisieren
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

    // Prueft ob Provisioning erfolgreich initialisiert wurde
    if (!nodeStatus.provisioning_bereit) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
        return;
    }

    // Intervalle aus persistierten Werten uebernehmen und validieren
    wendeReportIntervalAn(nodeProvisioning.sanitizeStatusSendInterval(nodeStatus.report_interval_s));
    nodeStatus.stored_sensor_send_interval_s =
        nodeProvisioning.sanitizeSensorSendInterval(nodeStatus.stored_sensor_send_interval_s);

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO", "config report_interval_s=%lu stored_sensor_send_interval_s=%lu",
         (unsigned long)nodeStatus.report_interval_s,
         (unsigned long)nodeStatus.stored_sensor_send_interval_s);

    // Prueft ob eine persistierte Master-MAC existiert
    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        nodeProvisioning.enterSetupMode();
        return;
    }

    // Master bekannt: Funk initialisieren und HELLO senden
    initialisiereFunk();
    sendeHello();
}

// =============================================================================
// HAUPTSCHLEIFE – loop() mit Provisioning-Update und periodischen Nachrichten
// =============================================================================
void loop() {
    // Provisioning-Controller periodisch updaten (Webserver, Button, Timer)
    nodeProvisioning.update();

    // Prueft ob Provisioning oder Setup laeuft – dann keine Funk-Kommunikation
    if (!nodeStatus.provisioning_bereit || nodeStatus.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    // Prueft ob Funk noch nicht initialisiert – nachholen
    if (!nodeStatus.funk_bereit) {
        initialisiereFunk();
    }

    const unsigned long jetzt = millis();

    // HELLO senden wenn Master noch nicht bekannt und Retry-Intervall abgelaufen
    if (!nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendeHello();
    }

    // HEARTBEAT senden wenn Master bekannt und Heartbeat-Intervall abgelaufen
    if (nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS) {
        sendeHeartbeat();
    }

    // Prueft ob STATE faellig ist (dirty-Flag gesetzt ODER Intervall abgelaufen)
    const bool stateFaellig =
        nodeStatus.master_bekannt &&
        nodeStatus.master_mac_gueltig &&
        (nodeStatus.state_report_offen ||
         (nodeStatus.state_interval_ms > 0UL &&
          (jetzt - nodeStatus.letzter_state_ms) >= nodeStatus.state_interval_ms));

    // STATE senden wenn faellig
    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
