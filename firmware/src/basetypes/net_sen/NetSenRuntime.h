// =============================================================================
// NetSenRuntime.h – NET-SEN Basistyp: Netz-Sensor (ESP-NOW Runtime)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_sen/NetSenRuntime.h
//
// Datei-Funktion:
//   Komplette ESP-NOW-Sensor-Implementierung als Header-only.
//   Untersuetzt Temperatur (temp_01c), Feuchte (hum_01pct), Lux,
//   Druck (pressure_pa), Gas (gas_ohm), AQI, TVOC, eCO2 und
//   Bewegung (motion). Bietet zwei STATE-Payload-Formate (Standard
//   und Extended) sowie Custom-Sensor-Hooks fuer konkrete Devices.
//   Inkludiert Task-Watchdog und optionale I2C-Basis-Initialisierung.
//
// Protokoll-Nachrichten:
//   Senden:   HELLO, HEARTBEAT, STATE (Sensor oder Extended-Gas),
//             EVENT (device-events), ACK
//   Empfangen: HELLO_ACK, CMD (STATE_REQUEST), CFG
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch, Doxygen-Stil)
//
// Abhaengigkeiten (Projekt-intern):
//   DeviceConfig.h, PinConfig.h, NetSenProvisioning.h,
//   lib/sh_protocol (DeviceTypes.h, Protocol.h)
// =============================================================================

#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <string.h>

#include "NetSenProvisioning.h"
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

// =============================================================================
// KONSTANTEN – Debug, Geraete-Identifikation, Timing, Ungueltigkeits-Marker
// =============================================================================

// Debug aktiv nur wenn sowohl global als auch geraetespezifisch eingeschaltet
constexpr bool DEBUG_LOKAL_AKTIV = DEVICE_DEBUG_AKTIV && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "NET-SEN";
constexpr char DATEI_VERSION[] = "0.2.0";
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Marker fuer "nicht gemessen/ungueltig" in STATE-Payloads
constexpr uint32_t NET_SEN_PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;   // Druck ungueltig
constexpr uint32_t NET_SEN_GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;    // Gas ungueltig
constexpr uint16_t NET_SEN_AIR_METRIC_UNGUELTIG = 0xFFFFU;      // AQI/TVOC/eCO2 ungueltig

// Task-Watchdog-Timeout (nach 8s ohne Reset wird das Geraet zurueckgesetzt)
constexpr uint32_t TASK_WDT_TIMEOUT_S = 8UL;
// I2C-Bus-Timeout fuer Wire.begin() (50ms)
constexpr uint16_t I2C_TIMEOUT_MS = 50U;

// Puffergroesse fuer Setup-AP-SSID (DEVICE_ID muss hineinpassen)
constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
static_assert(
    sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "NET_SEN_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");

// =============================================================================
// CUSTOM-SENSOR-HOOKS – Device-spezifische Erweiterungen
// =============================================================================
// Diese drei Bloecke koennen von konkreten Device-Implementierungen ersetzt
// werden. Wenn NET_SEN_DEVICE_HAS_CUSTOM_... auf 1 gesetzt wird, muessen die
// entsprechenden Funktionen vom Device bereitgestellt werden.
// SensorHook:       Temperatur, Feuchte, Lux, Bewegung, Fehler
// ExtendedStateHook: Druck, Gas, AQI, TVOC, eCO2
// EventHook:        Device-Events (z.B. Bewegungserkennung, Schwellwert)
// =============================================================================

#ifndef NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS
#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 0
#endif

#ifndef NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS
#define NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS 0
#endif

#ifndef NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS
#define NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS 0
#endif

// =============================================================================
// STRUKTUREN – SensorState und NodeState
// =============================================================================

// SensorState – Aktuelle Messwerte aller angeschlossenen Sensoren
//   temp_01c:   Temperatur in Zehntel-Grad (z.B. 235 = 23,5 Grad C)
//   hum_01pct:  Luftfeuchte in Zehntel-Prozent (z.B. 455 = 45,5%)
//   lux:        Umgebungslicht in Lux (0xFFFF = ungueltig)
//   pressure_pa: Luftdruck in Pascal (0xFFFFFFFF = ungueltig)
//   gas_ohm:    Gaswiderstand in Ohm (0xFFFFFFFF = ungueltig)
//   aqi:        Luftqualitaetsindex (0xFFFF = ungueltig)
//   tvoc_ppb:   Gesamtfluechtige organische Verbindungen in ppb
//   eco2_ppm:   CO2-Aequivalent in ppm
//   motion:     Bewegung erkannt (0/1)
//   fault:      Sensorfehler (true = Messung fehlgeschlagen)
struct SensorState {
    int16_t temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t motion;
    bool fault;
};

// NodeState – Zentraler Geraetezustand (Funk- und System-Status)
struct NodeState {
    bool provisioning_bereit;       // true = NodeProvisioning initialisiert
    bool setup_mode;                // true = Geraet im Setup-Modus
    bool setup_ap_aktiv;            // true = Setup-AP laeuft
    bool restart_pending;           // true = Neustart angefordert
    bool master_bekannt;            // true = HELLO_ACK vom Master empfangen
    bool master_mac_gueltig;        // true = Master-MAC provisioniert
    bool state_report_offen;        // true = STATE muss gesendet werden
    bool funk_bereit;               // true = ESP-NOW initialisiert
    unsigned long letztes_hello_ms;     // Zeitstempel letztes HELLO
    unsigned long letzter_heartbeat_ms; // Zeitstempel letzter HEARTBEAT
    unsigned long letzter_state_ms;     // Zeitstempel letzter STATE
    unsigned long restart_requested_at_ms; // Zeitstempel Restart
    unsigned long state_interval_ms;     // STATE-Intervall aus report_interval_s
    uint32_t report_interval_s;         // Aktuelles Report-Intervall (s)
    uint32_t stored_sensor_send_interval_s; // Gespeichertes Sensor-Intervall
    uint8_t master_mac[6];              // Provisionierte Master-MAC
    uint8_t naechste_seq;               // Naechste ESP-NOW-Sequenz
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE]; // Setup-AP-SSID
    SensorState sensor;                  // Aktuelle Sensorwerte
};

// Globale Instanz des Geraetezustands
NodeState nodeStatus = {};

// =============================================================================
// FORWARD-DEKLARATIONEN – Custom-Sensor-Hooks (werden von konkreten Devices ueberschrieben)
// =============================================================================

// Basis-Init und Poll fuer Standard-Sensoren (Temp, Feuchte, Lux, Motion)
#if NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(
    int16_t* temp_01c,
    uint16_t* hum_01pct,
    uint16_t* lux,
    uint8_t* motion,
    bool* fault);
#else
void netSenDeviceSensorInit() {}

bool netSenDeviceSensorPoll(
    int16_t* /*temp_01c*/,
    uint16_t* /*hum_01pct*/,
    uint16_t* /*lux*/,
    uint8_t* /*motion*/,
    bool* /*fault*/)
{
    return false;  // Keine Aenderung
}
#endif

// Basis-Init und Poll fuer erweiterte Sensoren (Druck, Gas, AQI, TVOC, eCO2)
#if NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS
void netSenDeviceExtendedStateInit();
bool netSenDeviceExtendedStatePoll(
    uint32_t* pressure_pa,
    uint32_t* gas_ohm,
    uint16_t* aqi,
    uint16_t* tvoc_ppb,
    uint16_t* eco2_ppm);
#else
void netSenDeviceExtendedStateInit() {}

bool netSenDeviceExtendedStatePoll(
    uint32_t* /*pressure_pa*/,
    uint32_t* /*gas_ohm*/,
    uint16_t* /*aqi*/,
    uint16_t* /*tvoc_ppb*/,
    uint16_t* /*eco2_ppm*/)
{
    return false;  // Keine Aenderung
}
#endif

// Poll fuer Device-Events (z.B. Schwellwertueberschreitung, Bewegung)
#if NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS
bool netSenDevicePollEvent(
    uint8_t* event_type,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2);
#else
bool netSenDevicePollEvent(
    uint8_t* /*event_type*/,
    uint8_t* /*trigger*/,
    uint8_t* /*param1*/,
    uint16_t* /*param2*/)
{
    return false;  // Kein Event
}
#endif

// =============================================================================
// HILFSFUNKTIONEN – Cap-Pruefung, Sensor-Defaults, Logging, Watchdog, Strings, MAC
// =============================================================================

// netSenVerwendetErweitertenState – Prueft ob Extended-State-Payload verwendet werden soll
//   true wenn DEVICE_CAPS Druck (SH_CAP_PRESSURE) oder AQI (SH_CAP_AQI) enthaelt.
bool netSenVerwendetErweitertenState() {
    return (DEVICE_CAPS & (SH_CAP_PRESSURE | SH_CAP_AQI)) != 0U;
}

// setzeSensorDefaults – Setzt alle Sensorwerte auf Ungueltig-Marker und fault=true
void setzeSensorDefaults(SensorState* sensor) {
    if (!sensor) return;
    sensor->temp_01c = INT16_MIN;
    sensor->hum_01pct = 0xFFFFU;
    sensor->lux = 0xFFFFU;
    sensor->pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    sensor->gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
    sensor->aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
    sensor->tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
    sensor->eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
    sensor->motion = 0U;
    sensor->fault = true;  // Solange keine erste Messung: Fehlerstatus
}

// logf – Formatiertes Logging (nur bei aktiviertem Debug)
//   Gibt formatierte Meldungen auf Serial aus mit Prefix [level].
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

// provisioningLog – Logging-Callback fuer das Provisioning-Framework
void provisioningLog(const char* level, const char* message) {
    if (!DEBUG_LOKAL_AKTIV || level == nullptr || message == nullptr) return;

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// initialisiereTaskWatchdog – Startet den Task-Watchdog (8s Timeout)
//   Bei Haengern >8s wird das Geraet automatisch zurueckgesetzt.
//   Muss periodisch via esp_task_wdt_reset() zurueckgesetzt werden.
void initialisiereTaskWatchdog() {
    esp_task_wdt_config_t wdtConfig = {};
    wdtConfig.timeout_ms = TASK_WDT_TIMEOUT_S * 1000;
    wdtConfig.trigger_panic = true;
    const esp_err_t initErr = esp_task_wdt_init(&wdtConfig);
    if (initErr != ESP_OK && initErr != ESP_ERR_INVALID_STATE) {
        logf("WARN", "Task-Watchdog Init fehlgeschlagen (err=%d)", (int)initErr);
        return;
    }

    const esp_err_t addErr = esp_task_wdt_add(nullptr);
    if (addErr != ESP_OK && addErr != ESP_ERR_INVALID_STATE) {
        logf("WARN", "Loop-Task konnte nicht beim Watchdog angemeldet werden (err=%d)", (int)addErr);
    }
}

// copyText – Sicheres Kopieren eines null-terminierten Strings
void copyText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    if (!source) {
        target[0] = '\0';
        return;
    }

    strncpy(target, source, targetSize - 1U);
    target[targetSize - 1U] = '\0';
}

// istBroadcastMac – Prueft ob eine MAC die Broadcast-Adresse ist
bool istBroadcastMac(const uint8_t* mac) {
    return mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0;
}

// senderIstProvisionierterMaster – Prueft ob der Sender der provisionierte Master ist
bool senderIstProvisionierterMaster(const uint8_t* senderMac) {
    return nodeStatus.master_mac_gueltig &&
           senderMac != nullptr &&
           memcmp(senderMac, nodeStatus.master_mac, sizeof(nodeStatus.master_mac)) == 0;
}

// holeHelloZielMac – Ziel-MAC fuer HELLO (Master oder Broadcast)
const uint8_t* holeHelloZielMac() {
    return nodeStatus.master_mac_gueltig ? nodeStatus.master_mac : BROADCAST_MAC;
}

// wendeReportIntervalAn – Setzt Report-Intervall und rechnet in ms um
void wendeReportIntervalAn(uint32_t wertS) {
    nodeStatus.report_interval_s = wertS;
    nodeStatus.state_interval_ms = (unsigned long)nodeStatus.report_interval_s * 1000UL;
}

// =============================================================================
// PROVISIONING-HANDLER – NetSenProvisioningHandler (keine zusaetzlichen Felder)
// =============================================================================
class NetSenProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
  public:
    const char* pageTitle() const override { return "NET-SEN Provisioning"; }
    const char* pageIntro() const override {
        return "Node-Basis fuer Master-Bindung und Statusintervall.";
    }
    const char* deviceSectionTitle() const override { return "NET-SEN-Spezifisch"; }
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
NetSenProvisioningHandler netSenProvisioningHandler;

// speichereReportIntervalMitRollback – Wendet neues Report-Intervall an (mit Rollback)
bool speichereReportIntervalMitRollback(uint32_t valueS) {
    // Prueft ob Intervall gueltig
    if (!nodeProvisioning.isSendIntervalValid(valueS)) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    nodeProvisioning.captureBasisSnapshot(basisSnapshot);

    wendeReportIntervalAn(valueS);
    nodeStatus.state_report_offen = true;

    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }

    // Rollback: alten Zustand wiederherstellen
    nodeProvisioning.restoreBasisSnapshot(basisSnapshot);
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    return false;
}

// =============================================================================
// SENSOR-MASKE – Dynamischer Maskenaufbau aus DEVICE_CAPS
// =============================================================================

// buildSensorMask – Baut eine 10-stellige Sensor-Maske aus den Faehigkeiten
//   Setzt Buchstaben an Position 0-3 je nach gesetzten CAPS-Bits:
//     T = Temperatur (SH_CAP_TEMP), H = Feuchte (SH_CAP_HUM),
//     L = Lux (SH_CAP_LUX), M = Motion (SH_CAP_MOTION)
//   Nicht vorhandene Sensoren: 'X'
void buildSensorMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXXXXXXX");
    if (targetSize < SH_SENSOR_MASK_LEN) return;

    target[0] = (DEVICE_CAPS & SH_CAP_TEMP) ? 'T' : 'X';
    target[1] = (DEVICE_CAPS & SH_CAP_HUM) ? 'H' : 'X';
    target[2] = (DEVICE_CAPS & SH_CAP_LUX) ? 'L' : 'X';
    target[3] = (DEVICE_CAPS & SH_CAP_MOTION) ? 'M' : 'X';
}

// buildInputMask – Baut Eingangs-Maske (Sensor-Only, keine Taster)
void buildInputMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXX");
}

// =============================================================================
// KOMMUNIKATION – ESP-NOW Senden: Peer, Paket, ACK
// =============================================================================

// stellePeerSicher – Registriert eine MAC als ESP-NOW-Peer
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

// sendePaket – Zentrale ESP-NOW-Sendefunktion (baut Header+CRC, sendet)
bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    if (!nodeStatus.funk_bereit || zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
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

// sendeAck – Sendet eine ACK-Bestaetigung
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

// sendeHello – Sendet HELLO zur Master-Anmeldung mit Sensor/Input-Maske
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

// sendeHeartbeat – Sendet HEARTBEAT an den Master
bool sendeHeartbeat() {
    if (!nodeStatus.master_bekannt || !nodeStatus.master_mac_gueltig) return false;

    SmartHome::HeartbeatPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.uptime_s = millis() / 1000UL;

    if (!sendePaket(nodeStatus.master_mac, SH_MSG_HEARTBEAT, &payload, sizeof(payload), "HEARTBEAT")) {
        return false;
    }

    nodeStatus.letzter_heartbeat_ms = millis();
    return true;
}

// sendeState – Sendet STATE (2 Formate je nach Caps: Standard oder Extended)
//   Standard:    SensorConfigStateReportPayload (temp, hum, lux, motion, fault)
//   Extended:    ExtendedSensorGasConfigStateReportPayload (+pressure, gas, aqi, tvoc, eco2)
//   Auswahl:    automatisch via netSenVerwendetErweitertenState()
bool sendeState() {
    if (!nodeStatus.master_bekannt || !nodeStatus.master_mac_gueltig) return false;

    const uint16_t reportIntervalS = (uint16_t)(nodeStatus.state_interval_ms / 1000UL);
    // Prueft ob erweiterter State noetig ist (Druck- oder AQI-Sensor vorhanden)
    if (netSenVerwendetErweitertenState()) {
        SmartHome::ExtendedSensorGasConfigStateReportPayload payload = {};
        copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
        payload.temp_01c = nodeStatus.sensor.temp_01c;
        payload.hum_01pct = nodeStatus.sensor.hum_01pct;
        payload.lux = nodeStatus.sensor.lux;
        payload.pressure_pa = nodeStatus.sensor.pressure_pa;
        payload.gas_ohm = nodeStatus.sensor.gas_ohm;
        payload.aqi = nodeStatus.sensor.aqi;
        payload.tvoc_ppb = nodeStatus.sensor.tvoc_ppb;
        payload.eco2_ppm = nodeStatus.sensor.eco2_ppm;
        payload.motion = nodeStatus.sensor.motion;
        payload.fault = nodeStatus.sensor.fault ? 1U : 0U;
        payload.report_interval_s = reportIntervalS;

        if (!sendePaket(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE_EXT_GAS")) {
            return false;
        }
    } else {
        SmartHome::SensorConfigStateReportPayload payload = {};
        copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
        payload.temp_01c = nodeStatus.sensor.temp_01c;
        payload.hum_01pct = nodeStatus.sensor.hum_01pct;
        payload.lux = nodeStatus.sensor.lux;
        payload.motion = nodeStatus.sensor.motion;
        payload.fault = nodeStatus.sensor.fault ? 1U : 0U;
        payload.report_interval_s = reportIntervalS;

        if (!sendePaket(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
            return false;
        }
    }

    nodeStatus.state_report_offen = false;
    nodeStatus.letzter_state_ms = millis();
    return true;
}

// sendeDeviceEvent – Sendet ein Device-Event (z.B. Bewegung erkannt)
bool sendeDeviceEvent(uint8_t eventType, uint8_t trigger, uint8_t param1, uint16_t param2) {
    if (!nodeStatus.master_bekannt || !nodeStatus.master_mac_gueltig) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = eventType;
    payload.trigger = trigger;
    payload.param1 = param1;
    payload.param2 = param2;

    return sendePaket(nodeStatus.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT");
}

// sendeAusstehendesDeviceEvent – Ruft den Event-Hook auf und sendet bei Event
void sendeAusstehendesDeviceEvent() {
    uint8_t eventType = 0U;
    uint8_t trigger = SH_TRIGGER_UNKNOWN;
    uint8_t param1 = 0U;
    uint16_t param2 = 0U;

    if (!netSenDevicePollEvent(&eventType, &trigger, &param1, &param2)) return;
    if (eventType == 0U) return;
    sendeDeviceEvent(eventType, trigger, param1, param2);
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG – HELLO_ACK, CMD, CFG
// =============================================================================

// verarbeiteHelloAck – Verarbeitet HELLO_ACK vom Master
void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    // Prueft ob ACK-Status OK ist
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    // Prueft ob Master-MAC provisioniert wurde
    if (!nodeStatus.master_mac_gueltig) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return;
    }

    // Prueft ob Sender der provisionierte Master ist
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    nodeStatus.master_bekannt = true;
    nodeStatus.state_report_offen = true;
    stellePeerSicher(nodeStatus.master_mac);
    sendeAusstehendesDeviceEvent();
    logf("INFO", "HELLO_ACK empfangen");
}

// verarbeiteCmd – Verarbeitet eingehende CMD (STATE_REQUEST)
void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::CmdPayload& payload) {
    // Prueft ob Sender der provisionierte Master ist
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    // STATE_REQUEST: dirty-Flag setzen
    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        nodeStatus.state_report_offen = true;
    }
}

// uebernehmeCfg – Wendet CFG-Wert an (nur report_interval unterstuetzt)
bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    // Prueft ob es sich um das Report-Intervall handelt
    if (payload.param_id != SH_CFG_REPORT_INTERVAL_S) return false;

    const bool ok = speichereReportIntervalMitRollback(payload.value);
    if (!ok) {
        logf("WARN", "report_interval_s konnte nicht uebernommen werden");
    }
    return ok;
}

// verarbeiteCfg – Verarbeitet CFG-Nachricht mit ACK-Unterstuetzung
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

// =============================================================================
// ESP-NOW – Paketverarbeitung, Callbacks, Funk-Initialisierung
// =============================================================================

// verarbeiteEspNowPaket – CRC-Pruefung + Dispatch an Handler (switch/msg_type)
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
                verarbeiteCmd(senderMac, *reinterpret_cast<const SmartHome::CmdPayload*>(payload));
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

// ESP-NOW Recv-Callback (Core v3 und v2)
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

// ESP-NOW Send-Callback (Warnung bei Fehlschlag)
void onEspNowSend(const wifi_tx_info_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

// initialisiereFunk – ESP-NOW initialisieren (WLAN, Callbacks, Peers)
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

// =============================================================================
// SENSORIK – I2C-Init, Sensor-Polling
// =============================================================================

// initialisiereSensorik – Startet I2C (falls aktiviert) und ruft Sensor-Init-Hooks auf
void initialisiereSensorik() {
    if (I2C_BASIS_AKTIV) {
        Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
        Wire.setTimeOut(I2C_TIMEOUT_MS);
    }

    netSenDeviceSensorInit();
    netSenDeviceExtendedStateInit();
}

// pollSensorik – Ruft alle Sensor-Hooks auf und aktualisiert nodeStatus.sensor
//   Setzt state_report_offen = true wenn sich Werte geaendert haben.
void pollSensorik() {
    SensorState neuerState = nodeStatus.sensor;
    bool geaendert = netSenDeviceSensorPoll(
        &neuerState.temp_01c,
        &neuerState.hum_01pct,
        &neuerState.lux,
        &neuerState.motion,
        &neuerState.fault);
    const bool extendedGeaendert = netSenDeviceExtendedStatePoll(
        &neuerState.pressure_pa,
        &neuerState.gas_ohm,
        &neuerState.aqi,
        &neuerState.tvoc_ppb,
        &neuerState.eco2_ppm);
    geaendert = geaendert || extendedGeaendert;

    nodeStatus.sensor = neuerState;
    if (geaendert) {
        nodeStatus.state_report_offen = true;
    }
}

// =============================================================================
// ARDUINO – setup() und loop()
// =============================================================================

// setup – Arduino-Hauptinitialisierung (Serial, Watchdog, Provisioning, Sensorik, Funk)
void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    initialisiereTaskWatchdog();

    // Status zuruecksetzen und Defaults laden
    nodeStatus = {};
    nodeStatus.report_interval_s = DEFAULT_REPORT_INTERVAL_S;
    nodeStatus.stored_sensor_send_interval_s = DEFAULT_SENSOR_SEND_INTERVAL_S;
    wendeReportIntervalAn(nodeStatus.report_interval_s);
    nodeStatus.state_report_offen = true;
    setzeSensorDefaults(&nodeStatus.sensor);

    // Optionale Status-LED initialisieren
    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, LOW);
    }

    // Provisioning konfigurieren und starten
    SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig =
        SmartHome::NetSenProvisioning::makeConfig(
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
        &netSenProvisioningHandler,
        provisioningLog);

    if (!nodeStatus.provisioning_bereit) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
        return;
    }

    // Intervalle aus persistierten Werten uebernehmen
    wendeReportIntervalAn(nodeProvisioning.sanitizeStatusSendInterval(nodeStatus.report_interval_s));
    nodeStatus.stored_sensor_send_interval_s =
        nodeProvisioning.sanitizeSensorSendInterval(nodeStatus.stored_sensor_send_interval_s);

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO", "config report_interval_s=%lu stored_sensor_send_interval_s=%lu",
         (unsigned long)nodeStatus.report_interval_s,
         (unsigned long)nodeStatus.stored_sensor_send_interval_s);

    // Prueft ob Master-MAC bereits provisioniert ist
    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        nodeProvisioning.enterSetupMode();
        return;
    }

    initialisiereSensorik();
    initialisiereFunk();
    sendeHello();
}

// loop – Hauptschleife: Watchdog-Reset, Provisioning, Sensor-Polling, Funk-Kommunikation
void loop() {
    esp_task_wdt_reset();
    nodeProvisioning.update();

    // Prueft ob Provisioning bereit und nicht im Setup-Modus
    if (!nodeStatus.provisioning_bereit || nodeStatus.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    // Prueft ob Funk bereit ist (ggf. nachinitialisieren)
    if (!nodeStatus.funk_bereit) {
        initialisiereFunk();
    }

    const unsigned long jetzt = millis();

    // Sensoren abfragen
    pollSensorik();

    // Ausstehende Device-Events senden (wenn Master bekannt)
    if (nodeStatus.master_bekannt && nodeStatus.master_mac_gueltig) {
        sendeAusstehendesDeviceEvent();
    }

    // HELLO senden wenn Master nicht bekannt und Retry-Intervall abgelaufen
    if (!nodeStatus.master_bekannt &&
        (nodeStatus.letztes_hello_ms == 0UL ||
         (jetzt - nodeStatus.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS)) {
        sendeHello();
    }

    // HEARTBEAT senden wenn faellig
    if (nodeStatus.master_bekannt &&
        (nodeStatus.letzter_heartbeat_ms == 0UL ||
         (jetzt - nodeStatus.letzter_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS)) {
        sendeHeartbeat();
    }

    // STATE-Sendung wenn dirty-Flag gesetzt oder Intervall abgelaufen
    const bool stateFaellig =
        nodeStatus.master_bekannt &&
        nodeStatus.master_mac_gueltig &&
        (nodeStatus.state_report_offen ||
         nodeStatus.letzter_state_ms == 0UL ||
         (nodeStatus.state_interval_ms > 0UL &&
          (jetzt - nodeStatus.letzter_state_ms) >= nodeStatus.state_interval_ms));

    if (stateFaellig) {
        sendeState();
    }

    delay(LOOP_INTERVAL_MS);
}
