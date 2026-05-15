// =============================================================================
// main.cpp – NET-ZRL Basistyp: Netz-Rollladen (ESP-NOW)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_zrl/main.cpp
//
// Datei-Funktion:
//   ESP-NOW-basierte Rollladen/Jalousie-Steuerung mit 2 Relais (Auf/Ab).
//   Vollstaendige Kalibrierungs-State-Machine, Positionsschaetzung waehrend
//   der Fahrt, Teil-Positionierung (set_position), 3-Tasten-Bedienung
//   (Up/Down/Stop) mit Hold-Gesten, LED-Rueckmeldung und serielle Konsole.
//   Integriert Web-Provisioning (Kalibrierwerte, Relais-Zuordnung) und
//   Factory-Reset.
//
// Protokoll-Nachrichten:
//   Senden:   HELLO, HEARTBEAT, STATE, EVENT (COVER_UP/DOWN/STOP/CALIB_START/DONE), ACK
//   Empfangen: HELLO_ACK, CMD (COVER-Kommandos, STATE_REQUEST), CFG
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch, Doxygen-Stil)
//
// Abhaengigkeiten (externe Libs):
//   Arduino.h, ShNodeProvisioning.h, WiFi.h, esp_now.h, esp_wifi.h
//
// Abhaengigkeiten (Projekt-intern):
//   NetZrlProvisioning.h, HardwarePinStandard.h, ProjectVersion.h,
//   lib/sh_protocol (DeviceTypes.h, Protocol.h)
// =============================================================================

#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NetZrlProvisioning.h"
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "../../../include/HardwarePinStandard.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"
#include "../../../include/MathUtils.h"
using SmartHome::clampToU16;
using SmartHome::absDiffU32;

// Alle Konstanten, Typen und Hilfsfunktionen liegen im anonymen Namespace
// (interne Kapselung, kein externer Linker-Zugriff).
namespace {

// =============================================================================
// KONSTANTEN – Geraete-Identifikation, Pins, Timing, Buffer
// =============================================================================

// Kurzbezeichnung des Geraetetyps fuer Log-Ausgaben
constexpr char DATEI_GERAET[] = "NET-ZRL";
// Firmware-Version als String
constexpr char DATEI_VERSION[] = "0.4.0";
// ESP-NOW Broadcast-Adresse (MAC FF:FF:FF:FF:FF:FF)
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#ifndef NET_ZRL_DEVICE_ID
#define NET_ZRL_DEVICE_ID "NET-ZRL-001"
#endif

#ifndef NET_ZRL_DEVICE_NAME
#define NET_ZRL_DEVICE_NAME "NET-ZRL Cover"
#endif

#ifndef NET_ZRL_FW_VARIANT
#define NET_ZRL_FW_VARIANT "net_zrl_base"
#endif

#ifndef NET_ZRL_DEBUG_ENABLED
#define NET_ZRL_DEBUG_ENABLED 1
#endif

#ifndef NET_ZRL_WLAN_CHANNEL
#define NET_ZRL_WLAN_CHANNEL 6
#endif

#ifndef NET_ZRL_HELLO_RETRY_INTERVAL_MS
#define NET_ZRL_HELLO_RETRY_INTERVAL_MS 5000UL
#endif

#ifndef NET_ZRL_HEARTBEAT_INTERVAL_MS
#define NET_ZRL_HEARTBEAT_INTERVAL_MS 20000UL
#endif

#ifndef NET_ZRL_BOOT_COUNTER
#define NET_ZRL_BOOT_COUNTER 1U
#endif

#ifndef NET_ZRL_RELAY_UP_PIN
#define NET_ZRL_RELAY_UP_PIN SmartHome::HardwarePinStandard::PIN_RELAY_1
#endif

#ifndef NET_ZRL_RELAY_DOWN_PIN
#define NET_ZRL_RELAY_DOWN_PIN SmartHome::HardwarePinStandard::PIN_RELAY_2
#endif

#ifndef NET_ZRL_RELAY_UP_ACTIVE_HIGH
#define NET_ZRL_RELAY_UP_ACTIVE_HIGH 1
#endif

#ifndef NET_ZRL_RELAY_DOWN_ACTIVE_HIGH
#define NET_ZRL_RELAY_DOWN_ACTIVE_HIGH 1
#endif

#ifndef NET_ZRL_BUTTON_UP_PIN
#define NET_ZRL_BUTTON_UP_PIN 2
#endif

#ifndef NET_ZRL_BUTTON_DOWN_PIN
#define NET_ZRL_BUTTON_DOWN_PIN 4
#endif

#ifndef NET_ZRL_BUTTON_STOP_PIN
#define NET_ZRL_BUTTON_STOP_PIN 3
#endif

#ifndef NET_ZRL_LED_UP_PIN
#define NET_ZRL_LED_UP_PIN 6
#endif

#ifndef NET_ZRL_LED_DOWN_PIN
#define NET_ZRL_LED_DOWN_PIN 7
#endif

#ifndef NET_ZRL_LED_ACTIVE_HIGH
#define NET_ZRL_LED_ACTIVE_HIGH 1
#endif

#ifndef NET_ZRL_BUTTON_ACTIVE_LOW
#define NET_ZRL_BUTTON_ACTIVE_LOW 0
#endif

constexpr bool DEBUG_AKTIV = NET_ZRL_DEBUG_ENABLED != 0;
constexpr char DEVICE_ID[] = NET_ZRL_DEVICE_ID;
constexpr char DEVICE_NAME[] = NET_ZRL_DEVICE_NAME;
constexpr char FW_VARIANT[] = NET_ZRL_FW_VARIANT;
constexpr uint16_t DEVICE_CAPS =
    (uint16_t)(SH_CAP_RELAY | SH_CAP_RELAY2 | SH_CAP_COVER | SH_CAP_MULTIBUTTON);
constexpr uint8_t DEVICE_META_SCHEMA_VERSION = SH_META_SCHEMA_VERSION_CURRENT;
constexpr uint8_t DEVICE_CONTROL_MODE = SH_CONTROL_MODE_COVER;
constexpr uint8_t DEVICE_CONFIG_PROFILE = SH_PROFILE_COVER_BASIC;
constexpr uint8_t DEVICE_REPORTING_MODE = SH_REPORTING_HYBRID;
constexpr int WLAN_KANAL = NET_ZRL_WLAN_CHANNEL;
constexpr unsigned long HELLO_RETRY_INTERVAL_MS = NET_ZRL_HELLO_RETRY_INTERVAL_MS;
constexpr unsigned long HEARTBEAT_INTERVAL_MS = NET_ZRL_HEARTBEAT_INTERVAL_MS;
constexpr unsigned long HELLO_REANNOUNCE_INTERVAL_MS = NET_ZRL_HELLO_RETRY_INTERVAL_MS;
constexpr uint32_t DEVICE_BOOT_COUNTER = NET_ZRL_BOOT_COUNTER;
constexpr int PIN_RELAY_A = NET_ZRL_RELAY_UP_PIN;
constexpr int PIN_RELAY_B = NET_ZRL_RELAY_DOWN_PIN;
constexpr bool RELAY_A_ACTIVE_HIGH = NET_ZRL_RELAY_UP_ACTIVE_HIGH != 0;
constexpr bool RELAY_B_ACTIVE_HIGH = NET_ZRL_RELAY_DOWN_ACTIVE_HIGH != 0;
constexpr int PIN_BUTTON_UP = NET_ZRL_BUTTON_UP_PIN;
constexpr int PIN_BUTTON_DOWN = NET_ZRL_BUTTON_DOWN_PIN;
constexpr int PIN_BUTTON_STOP = NET_ZRL_BUTTON_STOP_PIN;
constexpr int PIN_LED_UP = NET_ZRL_LED_UP_PIN;
constexpr int PIN_LED_DOWN = NET_ZRL_LED_DOWN_PIN;
constexpr bool LED_ACTIVE_HIGH = NET_ZRL_LED_ACTIVE_HIGH != 0;
constexpr bool BUTTON_ACTIVE_LOW = NET_ZRL_BUTTON_ACTIVE_LOW != 0;

constexpr unsigned long LOOP_DELAY_MS = 10UL;
constexpr unsigned long BUTTON_POLL_MS = 20UL;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 35UL;
constexpr unsigned long HOLD_MS = 5000UL;
constexpr unsigned long DEFAULT_ESTIMATED_TRAVEL_TIME_MS = 100000UL;
constexpr int16_t COVER_POSITION_UNBEKANNT = 255;
constexpr unsigned long LED_BLINK_INTERVAL_MS = 300UL;
constexpr unsigned long LED_SUCCESS_INTERVAL_MS = 180UL;
constexpr unsigned long LED_ACK_DURATION_MS = 180UL;
constexpr uint32_t MIN_TRAVEL_TIME_MS = 1000UL;
constexpr uint32_t MAX_TRAVEL_TIME_MS = 180000UL;
constexpr uint32_t DEFAULT_STATUS_SEND_INTERVAL_S = 60UL;
constexpr uint32_t DEFAULT_SENSOR_SEND_INTERVAL_S = 60UL;
constexpr uint32_t MIN_SEND_INTERVAL_S = 10UL;
constexpr uint32_t MAX_SEND_INTERVAL_S = 65535UL;
constexpr uint8_t CALIBRATION_SUCCESS_BLINK_PULSES = 3U;
constexpr uint8_t SETUP_CONFIRM_BLINK_PULSES = 3U;
constexpr uint8_t RESET_CONFIRM_BLINK_PULSES = 10U;
constexpr uint32_t RELAY_DEAD_TIME_MS = 300UL;  // Mindestpause zwischen Richtungswechsel
constexpr int NET_ZRL_WDT_TIMEOUT_S = 10;
constexpr size_t SERIAL_BUFFER_SIZE = 128U;
constexpr size_t MASTER_MAC_TEXT_LEN = SmartHome::ShNodeProvisioning::MASTER_MAC_TEXT_LEN;
constexpr size_t SETUP_SSID_BUFFER_SIZE = 32U;
constexpr const char* STORAGE_KEY_NET_ZRL_BLOB = "net_zrl_v1";
static_assert(
    sizeof(DEVICE_ID) <= SETUP_SSID_BUFFER_SIZE,
    "NET_ZRL_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");
constexpr uint32_t NET_ZRL_SETUP_MAGIC = 0x5A524C32UL;
constexpr uint16_t NET_ZRL_SETUP_VERSION = 1U;

// =============================================================================
// ENUMS – Zustaende, Richtungen, Kalibrier-Phasen, LED-Modi, Aktionen
// =============================================================================

// CoverState – Bewegungszustand des Rollladens
enum class CoverState : uint8_t { Stopped = 0, Moving = 1 };

// CoverDirection – Fahrtrichtung
enum class CoverDirection : uint8_t { None = 0, Up = 1, Down = 2 };

// CalibrationPhase – Zustand der Kalibrierungs-State-Machine
enum class CalibrationPhase : uint8_t {
    Idle = 0,             // Keine Kalibrierung aktiv
    MovingToTop,          // Faehrt in Ausgangslage ganz oben
    WaitForDownStart,     // Wartet auf Benutzerkommando zum Runterfahren
    MeasuringDown,        // Misst Fahrzeit nach unten
    WaitForUpStart,       // Wartet auf Benutzerkommando zum Hochfahren
    MeasuringUp,          // Misst Fahrzeit nach oben
    SuccessBlink          // Blinkt Erfolg, speichert dann Kalibrierwerte
};

// LedMode – LED-Anzeigemodus
enum class LedMode : uint8_t {
    Off = 0,     // Beide LEDs aus
    BothBlink,   // Beide blinken synchron (Kalibrierung aktiv)
    UpBlink,     // Nur Up-LED blinkt (warte auf Up-Start)
    DownBlink,   // Nur Down-LED blinkt (warte auf Down-Start)
    UpOn,        // Up-LED dauerhaft (Fahrt nach oben)
    DownOn       // Down-LED dauerhaft (Fahrt nach unten)
};

// PendingAction – Aktion nach Hold-Geste (wird nach Blinkmuster ausgefuehrt)
enum class PendingAction : uint8_t { None = 0, SetupEnter, FactoryReset };

// =============================================================================
// STRUKTUREN – Persistenz, Setup-Snapshots, Laufzeitzustand
// =============================================================================

// NetZrlPersistedSetupData – Roh-Persistenzdaten im NVS (Preferences)
struct NetZrlPersistedSetupData {
    uint32_t magic;                       // Magic zur Validierung (NET_ZRL_SETUP_MAGIC)
    uint16_t version;                     // Datenstruktur-Version
    uint16_t reserved;                    // Reserviert / Alignment
    uint32_t travelTimeUpMs;              // Kalibrierte Fahrzeit hoch (0=nicht kalibriert)
    uint32_t travelTimeDownMs;            // Kalibrierte Fahrzeit runter
    uint32_t defaultEstimatedTravelTimeMs;// Fallback-Fahrzeit
    uint8_t relayUpUsesRelayA;            // 1=RelaisA fuer Up, 0=RelaisB
    uint8_t reservedBytes[3];             // Reserviert / Alignment
};

// NetZrlSetupSnapshot – Snapshot fuer Rollback bei fehlgeschlagener Persistenz
struct NetZrlSetupSnapshot {
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    bool relayUpUsesRelayA;
};

// RuntimeState – Zentraler Geraetezustand (alle Laufzeitdaten, ~70 Felder)
// =============================================================================
struct RuntimeState {
    // ---- Bewegung ----
    CoverState coverState;                // Bewegungszustand (stopped/moving)
    CoverDirection coverDirection;        // Fahrtrichtung (none/up/down)
    bool relayAActive;                    // Relais A aktiv (HIGH)
    bool relayBActive;                    // Relais B aktiv (HIGH)
    unsigned long letzteRelaisWechselMs;    // Zeitstempel letzter Relaiswechsel (Dead-Time)
    bool relayUpUsesRelayA;               // Relais-Zuordnung: true=A=Up, false=B=Up
    bool masterMacValid;                  // Master-MAC wurde provisioniert
    uint8_t masterMac[6];                 // Provisionierte Master-MAC
    bool calibrationMode;                 // Kalibriermodus aktiv
    bool setupMode;                       // Setup-Modus aktiv
    bool isCalibrated;                    // Kalibrierung abgeschlossen
    bool setupApActive;                   // Setup-AP laeuft
    bool restartPending;                  // Neustart angefordert
    bool funkBereit;                      // ESP-NOW initialisiert
    bool masterBound;                     // HELLO_ACK vom Master empfangen
    bool stateReportOffen;                // STATE muss gesendet werden (dirty-Flag)
    bool movementTargetsIntermediatePosition; // Teil-Positionsfahrt
    int16_t coverPosition;                // Geschaetzte Position (0-100, 255=unbekannt)
    int16_t movementTargetPosition;       // Zielposition der aktuellen Fahrt
    uint32_t travelTimeUpMs;              // Kalibrierte Fahrzeit hoch
    uint32_t travelTimeDownMs;            // Kalibrierte Fahrzeit runter
    uint32_t candidateTravelTimeUpMs;     // Kandidat-Fahrzeit hoch (Messung)
    uint32_t candidateTravelTimeDownMs;   // Kandidat-Fahrzeit runter (Messung)
    uint32_t defaultEstimatedTravelTimeMs;// Fallback-Fahrzeit
    uint32_t statusSendIntervalS;         // STATE-Sendeintervall (Sekunden)
    uint32_t sensorSendIntervalS;         // Sensor-Intervall (ungenutzt)
    CalibrationPhase calibrationPhase;    // Aktuelle Kalibrierungsphase
    unsigned long movementStartedAtMs;    // Zeitstempel Fahrtbeginn
    unsigned long movementAutoStopMs;     // Auto-Stop-Dauer (0=kein)
    unsigned long restartRequestedAtMs;   // Zeitstempel Restart
    unsigned long letztesHelloMs;         // Zeitstempel letztes HELLO
    unsigned long letzterHeartbeatMs;     // Zeitstempel letzter HEARTBEAT
    unsigned long letzterStateMs;         // Zeitstempel letzter STATE
    int16_t movementStartPosition;        // Position bei Fahrtbeginn
    bool movementTargetsEndPosition;      // Fahrt zielt auf Endlage (0/100)
    uint8_t naechsteSeq;                  // Naechste ESP-NOW-Sequenz

    // ---- ACK-Tracking ----
    bool lastCmdAckValid;                 // Letzter CMD-ACK gueltig
    bool lastCfgAckValid;                 // Letzter CFG-ACK gueltig
    uint8_t lastCmdSeq;                   // Sequenz letzter bestaetigter CMD
    uint8_t lastCfgSeq;                   // Sequenz letzter bestaetigter CFG
    uint8_t lastCmdAckStatus;             // Status letzter CMD-ACK
    uint8_t lastCfgAckStatus;             // Status letzter CFG-ACK

    // ---- Taster-Entprellung ----
    unsigned long lastButtonPollMs;       // Zeitstempel letzte Abfrage
    bool upButtonStableActive;            // Up stabil (entprellt)
    bool downButtonStableActive;          // Down stabil (entprellt)
    bool stopButtonStableActive;          // Stop stabil (entprellt)
    bool upButtonRawActive;               // Up Rohwert
    bool downButtonRawActive;             // Down Rohwert
    bool stopButtonRawActive;             // Stop Rohwert
    unsigned long upButtonRawChangedAtMs; // Zeit letzte Raw-Aenderung Up
    unsigned long downButtonRawChangedAtMs;
    unsigned long stopButtonRawChangedAtMs;
    bool lastUpButtonActive;              // Letzter Up-Zustand
    bool lastDownButtonActive;
    bool lastStopButtonActive;
    unsigned long upPressedAtMs;          // Zeitstempel Up gedrueckt
    unsigned long downPressedAtMs;        // Zeitstempel Down gedrueckt
    unsigned long stopPressedAtMs;        // Zeitstempel Stop gedrueckt
    bool upHoldConsumed;                  // Up-Hold bereits verarbeitet
    bool downHoldConsumed;                // Down-Hold bereits verarbeitet
    bool stopHoldConsumed;                // Stop-Hold bereits verarbeitet

    // ---- LED-Steuerung ----
    LedMode ledMode;                      // Aktueller LED-Modus
    LedMode ledModeAfterAck;              // LED-Modus nach ACK
    bool ledBlinkState;                   // Blink-Zustand (on/off)
    unsigned long ledLastTickMs;          // Zeitstempel letzter LED-Tick
    bool ledAckActive;                    // ACK-LED laeuft
    unsigned long ledAckStartedAtMs;      // Startzeit ACK-LED
    uint8_t successBlinkToggleCount;      // Verbleibende Blink-Wechsel (Success)

    // ---- Ausstehende Aktionen ----
    PendingAction pendingAction;           // Ausstehende Aktion
    uint8_t pendingActionBlinkToggleCount; // Verbleibende Blink-Wechsel

    // ---- Setup / Serielle Konsole ----
    char setupApSsid[SETUP_SSID_BUFFER_SIZE];  // SSID Setup-AP
    char serialBuffer[SERIAL_BUFFER_SIZE];      // Serieller Eingabepuffer
    size_t serialLength;                        // Eingabelaenge
};

RuntimeState runtime = {};

// =============================================================================
// HILFSFUNKTIONEN – Logging, Enum-to-Text, Validierung, MAC, Strings
// =============================================================================

// logf – Formatiertes Logging (nur bei aktiviertem Debug)
void logf(const char* level, const char* format, ...) {
    if (!DEBUG_AKTIV) return;

    char message[240];
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
    if (!DEBUG_AKTIV || level == nullptr || message == nullptr) return;

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

void loggeButtonKante(const char* name, bool active, unsigned long heldMs) {
    logf("INFO", "Button %s %s (held_ms=%lu)", name, active ? "pressed" : "released", heldMs);
}

const char* toText(CoverState state) {
    return state == CoverState::Moving ? "moving" : "stopped";
}

const char* toText(CoverDirection direction) {
    switch (direction) {
        case CoverDirection::Up: return "up";
        case CoverDirection::Down: return "down";
        case CoverDirection::None:
        default: return "null";
    }
}

const char* toText(CalibrationPhase phase) {
    switch (phase) {
        case CalibrationPhase::MovingToTop: return "moving_to_top";
        case CalibrationPhase::WaitForDownStart: return "wait_for_down_start";
        case CalibrationPhase::MeasuringDown: return "measuring_down";
        case CalibrationPhase::WaitForUpStart: return "wait_for_up_start";
        case CalibrationPhase::MeasuringUp: return "measuring_up";
        case CalibrationPhase::SuccessBlink: return "success_blink";
        case CalibrationPhase::Idle:
        default: return "idle";
    }
}

const char* toText(PendingAction action) {
    switch (action) {
        case PendingAction::SetupEnter: return "setup_enter";
        case PendingAction::FactoryReset: return "factory_reset";
        case PendingAction::None:
        default: return "none";
    }
}

bool isTravelTimeValid(uint32_t valueMs) {
    return valueMs >= MIN_TRAVEL_TIME_MS && valueMs <= MAX_TRAVEL_TIME_MS;
}

bool isSendIntervalValid(uint32_t valueS) {
    return valueS >= MIN_SEND_INTERVAL_S && valueS <= MAX_SEND_INTERVAL_S;
}

uint32_t sanitizeEstimatedTravelTime(uint32_t valueMs) {
    return isTravelTimeValid(valueMs) ? valueMs : DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
}

uint32_t sanitizeStatusSendInterval(uint32_t valueS) {
    return isSendIntervalValid(valueS) ? valueS : DEFAULT_STATUS_SEND_INTERVAL_S;
}

uint32_t sanitizeSensorSendInterval(uint32_t valueS) {
    return isSendIntervalValid(valueS) ? valueS : DEFAULT_SENSOR_SEND_INTERVAL_S;
}

void clearStoredMasterMac() {
    runtime.masterMacValid = false;
    memset(runtime.masterMac, 0, sizeof(runtime.masterMac));
}

void setStoredMasterMac(const uint8_t masterMac[6]) {
    if (masterMac == nullptr) {
        clearStoredMasterMac();
        return;
    }
    runtime.masterMacValid = true;
    memcpy(runtime.masterMac, masterMac, sizeof(runtime.masterMac));
}

void formatMacText(const uint8_t mac[6], bool isValid, char* buffer, size_t bufferSize) {
    SmartHome::ShNodeProvisioning::NodeProvisioningController::formatMacText(
        mac,
        isValid,
        buffer,
        bufferSize);
}

bool parseMacText(const char* text, uint8_t outMac[6]) {
    return SmartHome::ShNodeProvisioning::NodeProvisioningController::parseMacText(text, outMac);
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

int16_t begrenzePosition(int32_t position) {
    if (position < 0) return 0;
    if (position > 100) return 100;
    return (int16_t)position;
}

bool istCoverPositionBekannt(int32_t position) {
    return position >= 0 && position <= 100;
}

void setzeCoverPositionUnbekannt() {
    runtime.coverPosition = COVER_POSITION_UNBEKANNT;
}

uint8_t coverPositionFuerPayload() {
    return istCoverPositionBekannt(runtime.coverPosition)
               ? (uint8_t)begrenzePosition(runtime.coverPosition)
               : 255U;
}

uint8_t coverStateCodeAusRuntime() {
    if (runtime.coverState != CoverState::Moving) return SH_COVER_STATE_STOPPED;
    if (runtime.coverDirection == CoverDirection::Up) return SH_COVER_STATE_MOVING_UP;
    if (runtime.coverDirection == CoverDirection::Down) return SH_COVER_STATE_MOVING_DOWN;
    return SH_COVER_STATE_STOPPED;
}

bool darfFunkAktivSein() {
    return !runtime.setupMode;
}

unsigned long stateIntervalMs() {
    return (unsigned long)sanitizeStatusSendInterval(runtime.statusSendIntervalS) * 1000UL;
}

void setzeStateReportOffen() {
    runtime.stateReportOffen = true;
}

void baueSensorMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    SmartHome::safeCopyMask("XXXXXXXXXX", target, targetSize, 'X');
}

void baueInputMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    SmartHome::safeCopyMask("BTN3X", target, targetSize, 'X');
}

// holeHelloZielMac – Bestimmt Ziel-MAC fuer HELLO (Master oder Broadcast)
const uint8_t* holeHelloZielMac() {
    return runtime.masterMacValid ? runtime.masterMac : BROADCAST_MAC;
}

// =============================================================================
// KOMMUNIKATION – ESP-NOW Senden: Peer, Paket, ACK, Sender-Validierung
// =============================================================================

// stellePeerSicher – Registriert eine MAC als ESP-NOW-Peer
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
    if (!runtime.funkBereit || zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t seq = runtime.naechsteSeq++;
    SmartHome::fillHeader(header, msgType, seq, flags, (uint16_t)payloadLen);

    uint8_t* payloadBuffer = packet + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(packet, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, packet, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label ? label : "ESP-NOW", (int)err);
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

// sendePaketMitRetry – Sendet mit bis zu 2 Wiederholungen bei Fehler
#ifndef NET_ZRL_ESPNOW_RETRY_COUNT
#define NET_ZRL_ESPNOW_RETRY_COUNT 2
#endif
#ifndef NET_ZRL_ESPNOW_RETRY_DELAY_MS
#define NET_ZRL_ESPNOW_RETRY_DELAY_MS 50UL
#endif

bool sendePaketMitRetry(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    if (runtime.naechsteSeq == 0) runtime.naechsteSeq = 255;
    else runtime.naechsteSeq--;

    for (int attempt = 0; attempt <= NET_ZRL_ESPNOW_RETRY_COUNT; attempt++) {
        if (sendePaket(zielMac, msgType, payload, payloadLen, label)) return true;
        if (attempt < NET_ZRL_ESPNOW_RETRY_COUNT) {
            logf("WARN", "%s Retry %d/%d", label, attempt + 1, NET_ZRL_ESPNOW_RETRY_COUNT);
            delay(NET_ZRL_ESPNOW_RETRY_DELAY_MS);
        }
    }
    logf("ERROR", "%s nach %d Versuchen fehlgeschlagen", label, NET_ZRL_ESPNOW_RETRY_COUNT + 1);
    return false;
}

bool sendeAck(const uint8_t* zielMac, uint8_t ackSeq, uint8_t ackMsgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = ackSeq;
    payload.ack_msg_type = ackMsgType;
    payload.status = status;
    return sendePaket(zielMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

bool senderIstBekannterMaster(const uint8_t* senderMac) {
    return runtime.masterMacValid &&
           senderMac != nullptr &&
           memcmp(senderMac, runtime.masterMac, sizeof(runtime.masterMac)) == 0;
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

String travelTimeText(uint32_t valueMs) {
    return valueMs > 0UL ? String(valueMs) : String();
}

// =============================================================================
// HARDWARE – GPIO lesen/schreiben, Taster entprellen, LEDs, Relais-Zuordnung
// =============================================================================

// leseButtonAktiv – Liest physischen Taster-Pin (beruecksichtigt active-HIGH/LOW)
bool leseButtonAktiv(int pin) {
    if (pin < 0) return false;
    const int raw = digitalRead(pin);
    return BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
}

bool entprelleButton(
    bool rawActive,
    bool& lastRawActive,
    unsigned long& changedAtMs,
    bool stableActive,
    unsigned long jetztMs) {
    if (rawActive != lastRawActive) {
        lastRawActive = rawActive;
        changedAtMs = jetztMs;
    }
    if (stableActive != rawActive && (jetztMs - changedAtMs) >= BUTTON_DEBOUNCE_MS) {
        return rawActive;
    }
    return stableActive;
}

void schreibePin(int pin, bool active, bool activeHigh) {
    if (pin >= 0) {
        digitalWrite(pin, active == activeHigh ? HIGH : LOW);
    }
}

void setzeLedPins(bool upOn, bool downOn) {
    schreibePin(PIN_LED_UP, upOn, LED_ACTIVE_HIGH);
    schreibePin(PIN_LED_DOWN, downOn, LED_ACTIVE_HIGH);
}

void setzeLedMode(LedMode mode) {
    runtime.ledMode = mode;
    runtime.ledBlinkState = false;
    runtime.ledLastTickMs = millis();
}

// istZeitfensterAbgelaufen – Prueft ob durationMs seit startedAtMs vergangen (millis-wrap-sicher)
bool istZeitfensterAbgelaufen(unsigned long startedAtMs, unsigned long durationMs, unsigned long jetztMs) {
    return durationMs > 0UL && (jetztMs - startedAtMs) >= durationMs;
}

void bestaetigeMitBeidenLeds(LedMode nextMode) {
    runtime.ledAckActive = true;
    runtime.ledAckStartedAtMs = millis();
    runtime.ledModeAfterAck = nextMode;
    setzeLedPins(true, true);
}

bool hatAusstehendeAktion() {
    return runtime.pendingAction != PendingAction::None;
}

void setzeLedNachNormalemStop() {
    // stoppeFahrt() kennt nur Relais- und Bewegungszustand.
    // Sonderpfade wie Kalibrierung oder Blink-Bestaetigungen muessen ihre LEDs bewusst behalten koennen.
    // Deshalb wird der LED-Reset nur in echten Normal-Stop-Pfaden explizit ausgeloest.
    runtime.ledAckActive = false;
    runtime.ledModeAfterAck = LedMode::Off;
    if (!runtime.setupMode && !runtime.calibrationMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
        setzeLedPins(false, false);
    }
}

bool resetBestaetigungAktiv() {
    return runtime.pendingAction == PendingAction::FactoryReset &&
           runtime.pendingActionBlinkToggleCount > 0U;
}

void starteAusstehendeAktion(PendingAction action, uint8_t blinkPulses) {
    if (action == PendingAction::None || blinkPulses == 0U || hatAusstehendeAktion()) return;
    runtime.pendingAction = action;
    runtime.pendingActionBlinkToggleCount = blinkPulses * 2U;
    runtime.ledBlinkState = false;
    runtime.ledLastTickMs = millis();
    setzeLedPins(false, false);
    logf("INFO",
         "Ausstehende Aktion gestartet (%s, blink_pulses=%u)",
         toText(action),
         blinkPulses);
}

void verwerfeAusstehendeAktion(const char* grund) {
    if (!hatAusstehendeAktion()) return;
    logf("INFO", "Ausstehende Aktion verworfen (%s)", grund ? grund : "ohne grund");
    runtime.pendingAction = PendingAction::None;
    runtime.pendingActionBlinkToggleCount = 0U;
    if (!runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(LedMode::Off);
    }
    setzeLedPins(false, false);
}

int pinFuerRichtung(CoverDirection direction) {
    const bool useRelayA =
        (direction == CoverDirection::Up) ? runtime.relayUpUsesRelayA : !runtime.relayUpUsesRelayA;
    return useRelayA ? PIN_RELAY_A : PIN_RELAY_B;
}

bool activeHighFuerRichtung(CoverDirection direction) {
    const bool useRelayA =
        (direction == CoverDirection::Up) ? runtime.relayUpUsesRelayA : !runtime.relayUpUsesRelayA;
    return useRelayA ? RELAY_A_ACTIVE_HIGH : RELAY_B_ACTIVE_HIGH;
}

// =============================================================================
// KALIBRIERUNG – Fahrzeiten berechnen, loeschen, persistieren
// =============================================================================

// berechneKalibrierstatus – Aktualisiert isCalibrated (beide Fahrzeiten gueltig)
void berechneKalibrierstatus() {
    runtime.isCalibrated =
        isTravelTimeValid(runtime.travelTimeUpMs) && isTravelTimeValid(runtime.travelTimeDownMs);
    if (!runtime.isCalibrated) {
        // Ohne Kalibrierung darf keine Zwischenlage erfunden werden.
        // Sichere 0/100-Endlagen entstehen nur nach sauber beendeter Vollfahrt.
        setzeCoverPositionUnbekannt();
    }
}

void loescheKalibrierungszustandImRuntime() {
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    runtime.isCalibrated = false;
    setzeCoverPositionUnbekannt();
    berechneKalibrierstatus();
}

bool parseUIntValue(const char* text, uint32_t& outValue);
void holeSetupSnapshot(
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    NetZrlSetupSnapshot& deviceSnapshot);
bool speicherePersistenzMitRollback(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const NetZrlSetupSnapshot& deviceSnapshot);

void holeNetZrlSetupSnapshot(NetZrlSetupSnapshot& snapshot) {
    snapshot.travelTimeUpMs = runtime.travelTimeUpMs;
    snapshot.travelTimeDownMs = runtime.travelTimeDownMs;
    snapshot.defaultEstimatedTravelTimeMs = runtime.defaultEstimatedTravelTimeMs;
    snapshot.relayUpUsesRelayA = runtime.relayUpUsesRelayA;
}

void wendeNetZrlSetupSnapshotAn(const NetZrlSetupSnapshot& snapshot) {
    runtime.travelTimeUpMs = snapshot.travelTimeUpMs;
    runtime.travelTimeDownMs = snapshot.travelTimeDownMs;
    runtime.defaultEstimatedTravelTimeMs = snapshot.defaultEstimatedTravelTimeMs;
    runtime.relayUpUsesRelayA = snapshot.relayUpUsesRelayA;
    berechneKalibrierstatus();
}

NetZrlPersistedSetupData baueNetZrlPersistenzdatenAusRuntime() {
    NetZrlPersistedSetupData data = {};
    data.magic = NET_ZRL_SETUP_MAGIC;
    data.version = NET_ZRL_SETUP_VERSION;
    data.travelTimeUpMs = isTravelTimeValid(runtime.travelTimeUpMs) ? runtime.travelTimeUpMs : 0UL;
    data.travelTimeDownMs = isTravelTimeValid(runtime.travelTimeDownMs) ? runtime.travelTimeDownMs : 0UL;
    data.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(runtime.defaultEstimatedTravelTimeMs);
    data.relayUpUsesRelayA = runtime.relayUpUsesRelayA ? 1U : 0U;
    return data;
}

bool netZrlPersistenzdatenSindGleich(
    const NetZrlPersistedSetupData& left,
    const NetZrlPersistedSetupData& right) {
    return left.magic == right.magic && left.version == right.version &&
           left.travelTimeUpMs == right.travelTimeUpMs &&
           left.travelTimeDownMs == right.travelTimeDownMs &&
           left.defaultEstimatedTravelTimeMs == right.defaultEstimatedTravelTimeMs &&
           left.relayUpUsesRelayA == right.relayUpUsesRelayA;
}

bool netZrlPersistenzdatenGueltig(const NetZrlPersistedSetupData& data) {
    return data.magic == NET_ZRL_SETUP_MAGIC && data.version == NET_ZRL_SETUP_VERSION;
}

void wendeNetZrlPersistenzdatenAn(const NetZrlPersistedSetupData& data) {
    runtime.travelTimeUpMs = isTravelTimeValid(data.travelTimeUpMs) ? data.travelTimeUpMs : 0UL;
    runtime.travelTimeDownMs = isTravelTimeValid(data.travelTimeDownMs) ? data.travelTimeDownMs : 0UL;
    runtime.defaultEstimatedTravelTimeMs =
        sanitizeEstimatedTravelTime(data.defaultEstimatedTravelTimeMs);
    runtime.relayUpUsesRelayA = data.relayUpUsesRelayA != 0U;
    berechneKalibrierstatus();
}

// =============================================================================
// PROVISIONING-HANDLER – Web-Konfiguration (NetZrlProvisioningHandler)
//   Stellt Kalibrierwerte und Relais-Zuordnung im Setup-Portal bereit.
//   Aktion: reset_calibration (Kalibrierung loeschen mit Rollback).
// =============================================================================
class NetZrlProvisioningHandler final : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
  public:
    const char* pageTitle() const override { return "NET-ZRL Provisioning"; }
    const char* pageIntro() const override { return "Node-Basis oben, Rolladenwerte unten."; }
    const char* deviceSectionTitle() const override { return "NET-ZRL-Spezifisch"; }
    const char* deviceSectionIntro() const override { return "Fahrzeiten setzen, Kalibrierung separat loeschen."; }

    void loadDeviceDefaults() override {
        runtime.travelTimeUpMs = 0UL;
        runtime.travelTimeDownMs = 0UL;
        runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
        runtime.relayUpUsesRelayA = true;
        berechneKalibrierstatus();
    }

    bool loadDeviceSettings(Preferences& prefs) override {
        NetZrlPersistedSetupData data = {};
        if (prefs.getBytesLength(STORAGE_KEY_NET_ZRL_BLOB) == sizeof(NetZrlPersistedSetupData) &&
            prefs.getBytes(STORAGE_KEY_NET_ZRL_BLOB, &data, sizeof(data)) == sizeof(data) &&
            netZrlPersistenzdatenGueltig(data)) {
            wendeNetZrlPersistenzdatenAn(data);
            return true;
        }
        return false;
    }

    bool saveDeviceSettings(Preferences& prefs) override {
        const NetZrlPersistedSetupData data = baueNetZrlPersistenzdatenAusRuntime();
        NetZrlPersistedSetupData current = {};
        if (prefs.getBytesLength(STORAGE_KEY_NET_ZRL_BLOB) == sizeof(NetZrlPersistedSetupData) &&
            prefs.getBytes(STORAGE_KEY_NET_ZRL_BLOB, &current, sizeof(current)) == sizeof(current) &&
            netZrlPersistenzdatenGueltig(current) &&
            netZrlPersistenzdatenSindGleich(current, data)) {
            return true;
        }

        return prefs.putBytes(STORAGE_KEY_NET_ZRL_BLOB, &data, sizeof(data)) == sizeof(data);
    }

    bool clearDeviceSettings(Preferences& prefs) override {
        prefs.remove(STORAGE_KEY_NET_ZRL_BLOB);
        return true;
    }

    void captureDeviceSnapshot() override { holeNetZrlSetupSnapshot(snapshot_); }
    void restoreDeviceSnapshot() override { wendeNetZrlSetupSnapshotAn(snapshot_); }

    bool parseDeviceSave(WebServer& server, String& errorText) override {
        const String travelTimeUpText = server.arg("travel_time_up_ms");
        const String travelTimeDownText = server.arg("travel_time_down_ms");
        const String defaultTravelTimeText = server.arg("default_estimated_travel_time_ms");
        const String relayMappingText = server.arg("relay_up_mapping");

        pending_ = {};

        if (!parseWebTravelTimeField(
                travelTimeUpText,
                runtime.travelTimeUpMs,
                pending_.travelTimeUpMs)) {
            errorText = F(
                "travel_time_up_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms. "
                "Leer laesst den aktuellen Wert unveraendert.");
            return false;
        }

        if (!parseWebTravelTimeField(
                travelTimeDownText,
                runtime.travelTimeDownMs,
                pending_.travelTimeDownMs)) {
            errorText = F(
                "travel_time_down_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms. "
                "Leer laesst den aktuellen Wert unveraendert.");
            return false;
        }

        if (!parsePflichtZahlFeld(
                defaultTravelTimeText,
                MIN_TRAVEL_TIME_MS,
                MAX_TRAVEL_TIME_MS,
                pending_.defaultEstimatedTravelTimeMs)) {
            errorText =
                F("default_estimated_travel_time_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms.");
            return false;
        }

        if (relayMappingText != "relay_a" && relayMappingText != "relay_b") {
            errorText = F("relay_up_mapping ist ungueltig.");
            return false;
        }

        pending_.relayUpUsesRelayA = relayMappingText == "relay_a";
        pending_.valid = true;
        return true;
    }

    void applyParsedDeviceSettings() override {
        if (!pending_.valid) return;

        runtime.travelTimeUpMs = pending_.travelTimeUpMs;
        runtime.travelTimeDownMs = pending_.travelTimeDownMs;
        berechneKalibrierstatus();
        runtime.defaultEstimatedTravelTimeMs =
            sanitizeEstimatedTravelTime(pending_.defaultEstimatedTravelTimeMs);
        runtime.relayUpUsesRelayA = pending_.relayUpUsesRelayA;
    }

    void discardParsedDeviceSettings() override { pending_ = {}; }

    void appendDeviceFieldsHtml(String& page, WebServer* sourceServer) const override {
        const String travelTimeUpText =
            sourceServer != nullptr && sourceServer->hasArg("travel_time_up_ms")
                ? sourceServer->arg("travel_time_up_ms")
                : travelTimeText(runtime.travelTimeUpMs);
        const String travelTimeDownText =
            sourceServer != nullptr && sourceServer->hasArg("travel_time_down_ms")
                ? sourceServer->arg("travel_time_down_ms")
                : travelTimeText(runtime.travelTimeDownMs);
        const String defaultTravelTimeText =
            sourceServer != nullptr && sourceServer->hasArg("default_estimated_travel_time_ms")
                ? sourceServer->arg("default_estimated_travel_time_ms")
                : String(runtime.defaultEstimatedTravelTimeMs);
        const String relayMappingText =
            sourceServer != nullptr && sourceServer->hasArg("relay_up_mapping")
                ? sourceServer->arg("relay_up_mapping")
                : String(runtime.relayUpUsesRelayA ? "relay_a" : "relay_b");
        const bool relayASelected = relayMappingText != "relay_b";
        const String escapedTravelTimeUp = htmlEscapeLocal(travelTimeUpText);
        const String escapedTravelTimeDown = htmlEscapeLocal(travelTimeDownText);
        const String escapedDefaultTravelTime = htmlEscapeLocal(defaultTravelTimeText);
        const String calibrationState = runtime.isCalibrated ? String(F("Kalibriert")) : String(F("Nicht kalibriert"));

        page += F("<div class=\"field\"><label for=\"travel_time_up_ms\">travel_time_up_ms</label>");
        page += F("<input id=\"travel_time_up_ms\" name=\"travel_time_up_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" placeholder=\"leer = unveraendert\" value=\"");
        page += escapedTravelTimeUp;
        page += F("\"><div class=\"hint\">Aktuell nur Zahl setzen.</div></div>");
        page += F("<div class=\"field\"><label for=\"travel_time_down_ms\">travel_time_down_ms</label>");
        page += F("<input id=\"travel_time_down_ms\" name=\"travel_time_down_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" placeholder=\"leer = unveraendert\" value=\"");
        page += escapedTravelTimeDown;
        page += F("\"><div class=\"hint\">Aktuell nur Zahl setzen.</div></div>");
        page += F("<div class=\"field\"><label for=\"default_estimated_travel_time_ms\">default_estimated_travel_time_ms</label>");
        page += F("<input id=\"default_estimated_travel_time_ms\" name=\"default_estimated_travel_time_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" value=\"");
        page += escapedDefaultTravelTime;
        page += F("\"><div class=\"hint\">Fallbackfahrt bei Start.</div></div>");
        page += F("<div class=\"field\"><label for=\"relay_up_mapping\">Relais-Zuordnung fuer Richtung up</label>");
        page += F("<select id=\"relay_up_mapping\" name=\"relay_up_mapping\">");
        page += relayASelected ? F("<option value=\"relay_a\" selected>relay_a = up</option>")
                               : F("<option value=\"relay_a\">relay_a = up</option>");
        page += relayASelected ? F("<option value=\"relay_b\">relay_b = up</option>")
                               : F("<option value=\"relay_b\" selected>relay_b = up</option>");
        page += F("</select><div class=\"hint\">Status: ");
        page += htmlEscapeLocal(calibrationState);
        page += F("</div></div>");
    }

    void appendDeviceActionsHtml(String& page) const override {
        page += F("<form class=\"card\" method=\"post\" action=\"/save\">");
        page += F("<div class=\"section-head\"><h2>Kalibrierung</h2><span class=\"tag\">reset</span></div>");
        page += F("<div class=\"sub\">Loescht nur die gespeicherten Fahrzeiten.</div>");
        page += F("<div class=\"meta\"><span>up <code>");
        page += htmlEscapeLocal(travelTimeText(runtime.travelTimeUpMs).length() > 0U ? travelTimeText(runtime.travelTimeUpMs) : String(F("-")));
        page += F("</code></span><span>down <code>");
        page += htmlEscapeLocal(travelTimeText(runtime.travelTimeDownMs).length() > 0U ? travelTimeText(runtime.travelTimeDownMs) : String(F("-")));
        page += F("</code></span></div>");
        page += F("<input type=\"hidden\" name=\"device_action\" value=\"reset_calibration\">");
        page += F("<div class=\"actions\"><button class=\"btn btn-danger\" type=\"submit\">Kalibrierung loeschen</button>");
        page += F("<div class=\"footer\">Basisdaten und Relais-Zuordnung bleiben unberuehrt.</div></div></form>");
    }

    bool handleDeviceAction(
        WebServer& server,
        String& titleText,
        String& messageText,
        int& statusCode,
        bool& restartRequired) override {
        restartRequired = false;

        if (server.arg("device_action") != "reset_calibration") {
            titleText = F("Aktion ungueltig");
            messageText = F("Die angeforderte NET-ZRL-Aktion wird nicht unterstuetzt.");
            statusCode = 400;
            return false;
        }

        SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
        NetZrlSetupSnapshot previousDeviceSnapshot = {};
        holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

        loescheKalibrierungszustandImRuntime();
        if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
            titleText = F("Loeschen fehlgeschlagen");
            messageText = F("Kalibrierung konnte nicht gespeichert geloescht werden.");
            statusCode = 500;
            return false;
        }

        titleText = F("Kalibrierung geloescht");
        messageText = F("Fahrzeiten entfernt. Setup bleibt offen.");
        statusCode = 200;
        return true;
    }

  private:
    struct PendingValues {
        bool valid;
        uint32_t travelTimeUpMs;
        uint32_t travelTimeDownMs;
        uint32_t defaultEstimatedTravelTimeMs;
        bool relayUpUsesRelayA;
    };

    PendingValues pending_ = {};
    NetZrlSetupSnapshot snapshot_ = {};

    static bool parsePflichtZahlFeld(
        const String& rawText,
        uint32_t minValue,
        uint32_t maxValue,
        uint32_t& outValue) {
        if (rawText.length() == 0U) return false;
        return parseUIntValue(rawText.c_str(), outValue) && outValue >= minValue &&
               outValue <= maxValue;
    }

    static bool parseWebTravelTimeField(
        const String& rawText,
        uint32_t currentValue,
        uint32_t& outValue) {
        if (rawText.length() == 0U) {
            outValue = currentValue;
            return true;
        }

        uint32_t parsedValue = 0UL;
        if (!parseUIntValue(rawText.c_str(), parsedValue) || !isTravelTimeValid(parsedValue)) {
            return false;
        }

        outValue = parsedValue;
        return true;
    }
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
NetZrlProvisioningHandler netZrlProvisioningHandler;

void holeSetupSnapshot(
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    NetZrlSetupSnapshot& deviceSnapshot) {
    nodeProvisioning.captureBasisSnapshot(basisSnapshot);
    holeNetZrlSetupSnapshot(deviceSnapshot);
}

void wendeSetupSnapshotAn(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const NetZrlSetupSnapshot& deviceSnapshot) {
    nodeProvisioning.restoreBasisSnapshot(basisSnapshot);
    wendeNetZrlSetupSnapshotAn(deviceSnapshot);
}

bool speicherePersistenzMitRollback(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& basisSnapshot,
    const NetZrlSetupSnapshot& deviceSnapshot) {
    if (nodeProvisioning.saveCurrentState()) {
        return true;
    }

    wendeSetupSnapshotAn(basisSnapshot, deviceSnapshot);
    return false;
}

bool speichereRuntimeStandMitRollback() {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot basisSnapshot = {};
    NetZrlSetupSnapshot deviceSnapshot = {};
    holeSetupSnapshot(basisSnapshot, deviceSnapshot);
    return speicherePersistenzMitRollback(basisSnapshot, deviceSnapshot);
}

bool uebernehmeMasterMacNachHelloAck(const uint8_t senderMac[6]) {
    if (senderMac == nullptr || !SmartHome::isValidMac(senderMac)) return false;

    if (!runtime.masterMacValid) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return false;
    }

    if (memcmp(runtime.masterMac, senderMac, sizeof(runtime.masterMac)) != 0) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return false;
    }

    runtime.masterBound = true;
    return true;
}

bool sendeHello() {
    if (!darfFunkAktivSein()) return false;

    SmartHome::HelloPayload payload = {};
    char sensorMask[SH_SENSOR_MASK_LEN] = {0};
    char inputMask[SH_INPUT_MASK_LEN] = {0};

    baueSensorMask(sensorMask, sizeof(sensorMask));
    baueInputMask(inputMask, sizeof(inputMask));

    copyText(payload.device_id, sizeof(payload.device_id), DEVICE_ID);
    copyText(payload.device_name, sizeof(payload.device_name), DEVICE_NAME);
    payload.device_class = SH_CLASS_NET_ZRL;
    payload.caps_hi = (uint8_t)((DEVICE_CAPS >> 8) & 0xFFU);
    payload.caps_lo = (uint8_t)(DEVICE_CAPS & 0xFFU);
    payload.power_type = SH_POWER_MAINS;
    payload.fw_version = 1U;
    payload.boot_counter = DEVICE_BOOT_COUNTER;
    payload.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    payload.control_mode = DEVICE_CONTROL_MODE;
    payload.config_profile = DEVICE_CONFIG_PROFILE;
    payload.reporting_mode = DEVICE_REPORTING_MODE;
    copyText(payload.sensor_mask, sizeof(payload.sensor_mask), sensorMask);
    copyText(payload.input_mask, sizeof(payload.input_mask), inputMask);

    runtime.letztesHelloMs = millis();
    return sendePaketMitRetry(holeHelloZielMac(), SH_MSG_HELLO, &payload, sizeof(payload), "HELLO");
}

bool sendeHeartbeat() {
    if (!runtime.masterBound || !darfFunkAktivSein()) return false;

    SmartHome::HeartbeatPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.uptime_s = millis() / 1000UL;

    if (!sendePaketMitRetry(runtime.masterMac, SH_MSG_HEARTBEAT, &payload, sizeof(payload), "HEARTBEAT")) {
        return false;
    }

    runtime.letzterHeartbeatMs = millis();
    return true;
}

bool sendeState() {
    if (!runtime.masterBound || !darfFunkAktivSein()) return false;

    SmartHome::ZrlConfigStateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.relay_1 = runtime.relayAActive ? 1U : 0U;
    payload.relay_2 = runtime.relayBActive ? 1U : 0U;
    payload.cover_mode = 1U;
    payload.cover_state = coverStateCodeAusRuntime();
    payload.cover_position = coverPositionFuerPayload();
    payload.cover_calibrated = runtime.isCalibrated ? 1U : 0U;
    payload.fault = 0U;
    payload.report_interval_s = (uint16_t)sanitizeStatusSendInterval(runtime.statusSendIntervalS);

    if (!sendePaketMitRetry(runtime.masterMac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    runtime.stateReportOffen = false;
    runtime.letzterStateMs = millis();
    return true;
}

bool sendeCoverEvent(uint8_t eventType, uint8_t trigger, uint8_t param1, uint16_t param2) {
    if (!runtime.masterBound || !darfFunkAktivSein()) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = eventType;
    payload.trigger = trigger;
    payload.param1 = param1;
    payload.param2 = param2;
    return sendePaketMitRetry(runtime.masterMac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT");
}

// =============================================================================
// BEWEGUNGSSTEUERUNG – Relais setzen, Fahrt starten/stoppen, Position schaetzen
// =============================================================================

// setzeRelaisNeutral – Schaltet beide Relais aus (sichere Ruhestellung)
void setzeRelaisNeutral(const char* grund) {
    schreibePin(PIN_RELAY_A, false, RELAY_A_ACTIVE_HIGH);
    schreibePin(PIN_RELAY_B, false, RELAY_B_ACTIVE_HIGH);
    runtime.relayAActive = false;
    runtime.relayBActive = false;
    logf("INFO", "Relais neutral (%s)", grund ? grund : "ohne grund");
}

void setzeRelaisFuerRichtung(CoverDirection direction, const char* grund) {
    if (direction == CoverDirection::None) {
        setzeRelaisNeutral("ungueltige richtung");
        return;
    }

    // Dead-Time: Verhindere schnellen Richtungswechsel (Schutz vor Motor-Kurzschluss)
    const unsigned long jetzt = millis();
    const unsigned long seitLetztemWechsel = jetzt - runtime.letzteRelaisWechselMs;
    if (runtime.letzteRelaisWechselMs > 0 && seitLetztemWechsel < RELAY_DEAD_TIME_MS) {
        const unsigned long restMs = RELAY_DEAD_TIME_MS - seitLetztemWechsel;
        logf("WARN", "Relais-Dead-Time: %lu ms warten vor Richtungswechsel", restMs);
        delay(restMs);
    }

    schreibePin(PIN_RELAY_A, false, RELAY_A_ACTIVE_HIGH);
    schreibePin(PIN_RELAY_B, false, RELAY_B_ACTIVE_HIGH);
    runtime.relayAActive = false;
    runtime.relayBActive = false;

    const int pin = pinFuerRichtung(direction);
    schreibePin(pin, true, activeHighFuerRichtung(direction));
    runtime.relayAActive = pin == PIN_RELAY_A;
    runtime.relayBActive = pin == PIN_RELAY_B;

    runtime.letzteRelaisWechselMs = millis();
    logf("INFO", "Relais fuer %s aktiv (%s)", toText(direction), grund ? grund : "ohne grund");
}

uint32_t fahrzeitFuerRichtung(CoverDirection direction) {
    if (direction == CoverDirection::Up) return runtime.travelTimeUpMs;
    if (direction == CoverDirection::Down) return runtime.travelTimeDownMs;
    return 0UL;
}

void aktualisierePositionsschaetzung(unsigned long jetztMs) {
    if (!runtime.isCalibrated) return;

    if (runtime.coverState != CoverState::Moving || runtime.coverDirection == CoverDirection::None) return;

    const uint32_t fahrzeitMs = fahrzeitFuerRichtung(runtime.coverDirection);
    if (!isTravelTimeValid(fahrzeitMs)) return;

    const unsigned long elapsedMs = jetztMs - runtime.movementStartedAtMs;
    const long delta = (long)((elapsedMs * 100UL) / fahrzeitMs);
    long nextPosition = runtime.movementStartPosition;
    nextPosition += runtime.coverDirection == CoverDirection::Up ? delta : -delta;

    if (nextPosition < 0L) nextPosition = 0L;
    if (nextPosition > 100L) nextPosition = 100L;
    runtime.coverPosition = (int16_t)nextPosition;
}

void stoppeFahrt(const char* grund, bool unkalibrierteEndlageBestaetigt = false) {
    aktualisierePositionsschaetzung(millis());
    if (!runtime.isCalibrated && !unkalibrierteEndlageBestaetigt) {
        // Nach manuellem Stop oder unklarem Abbruch bleibt die unkalibrierte Lage unbekannt.
        setzeCoverPositionUnbekannt();
    }
    setzeRelaisNeutral(grund);
    runtime.coverState = CoverState::Stopped;
    runtime.coverDirection = CoverDirection::None;
    runtime.movementStartedAtMs = 0UL;
    runtime.movementAutoStopMs = 0UL;
    runtime.movementStartPosition = runtime.coverPosition;
    runtime.movementTargetsEndPosition = false;
    runtime.movementTargetsIntermediatePosition = false;
    runtime.movementTargetPosition = runtime.coverPosition;
    setzeStateReportOffen();
}

void setzePositionAufEndlage(CoverDirection direction) {
    runtime.coverPosition = direction == CoverDirection::Up ? 100 : 0;
}

bool starteFahrt(CoverDirection direction, const char* grund, unsigned long autoStopMs, bool targetsEnd) {
    if (direction == CoverDirection::None || runtime.coverState == CoverState::Moving) return false;

    runtime.coverDirection = direction;
    runtime.coverState = CoverState::Moving;
    runtime.movementStartedAtMs = millis();
    runtime.movementAutoStopMs = autoStopMs;
    runtime.movementStartPosition =
        runtime.isCalibrated && istCoverPositionBekannt(runtime.coverPosition) ? runtime.coverPosition : 0;
    runtime.movementTargetsEndPosition = targetsEnd;
    runtime.movementTargetsIntermediatePosition = false;
    runtime.movementTargetPosition = runtime.coverPosition;
    if (!runtime.isCalibrated) {
        // Ohne Kalibrierung verlaesst die Fahrt die letzte sichere Endlage sofort.
        // Bis zum normalen Fahrtende bleibt die Position deshalb unbekannt.
        setzeCoverPositionUnbekannt();
        runtime.movementTargetPosition = COVER_POSITION_UNBEKANNT;
    }
    setzeRelaisFuerRichtung(direction, grund);
    setzeStateReportOffen();
    return true;
}

bool startePositionsfahrt(int16_t zielPosition, const char* grund) {
    if (!runtime.isCalibrated) return false;
    if (runtime.coverState == CoverState::Moving) return false;

    aktualisierePositionsschaetzung(millis());
    const int16_t aktuellePosition = begrenzePosition(runtime.coverPosition);
    const int16_t gekapptesZiel = begrenzePosition(zielPosition);
    if (gekapptesZiel == aktuellePosition) {
        runtime.coverPosition = gekapptesZiel;
        setzeStateReportOffen();
        return true;
    }

    const CoverDirection richtung =
        gekapptesZiel > aktuellePosition ? CoverDirection::Up : CoverDirection::Down;
    const uint32_t fahrzeitMs = fahrzeitFuerRichtung(richtung);
    if (!isTravelTimeValid(fahrzeitMs)) return false;

    const uint32_t deltaProzent = (uint32_t)abs((int)(gekapptesZiel - aktuellePosition));
    unsigned long segmentMs = (unsigned long)((fahrzeitMs * deltaProzent) / 100UL);
    if (segmentMs == 0UL) segmentMs = 1UL;

    if (!starteFahrt(richtung, grund, segmentMs, false)) return false;

    runtime.movementTargetsIntermediatePosition = true;
    runtime.movementTargetPosition = gekapptesZiel;
    if (!runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(richtung == CoverDirection::Up ? LedMode::UpOn : LedMode::DownOn);
    }
    return true;
}

void stoppeFahrtMitEvent(const char* grund, uint8_t trigger) {
    if (runtime.coverState != CoverState::Moving) return;
    stoppeFahrt(grund, false);
    sendeCoverEvent(SH_EVENT_COVER_STOP, trigger, coverPositionFuerPayload(), 0U);
}

void setzeKalibrierungUngueltig() {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    loescheKalibrierungszustandImRuntime();
    setzeStateReportOffen();

    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        logf("WARN", "Kalibrierung konnte nicht geloescht werden, Vorzustand wiederhergestellt");
    }
}

void enterSetupMode() {
    if (runtime.setupMode || runtime.coverState == CoverState::Moving) return;
    setzeLedMode(LedMode::Off);
    setzeLedPins(false, false);
    runtime.masterBound = false;
    if (runtime.funkBereit) {
        esp_now_deinit();
        runtime.funkBereit = false;
    }
    nodeProvisioning.enterSetupMode();
    if (runtime.setupMode && runtime.setupApActive) {
        logf("INFO", "Setup-Modus aktiviert");
    } else {
        logf("WARN", "Setup-Modus konnte nicht sauber aktiviert werden");
    }
}

void exitSetupMode(const char* grund) {
    nodeProvisioning.exitSetupMode(grund);
    runtime.masterBound = false;
    runtime.funkBereit = false;
    runtime.letztesHelloMs = 0UL;
    runtime.letzterHeartbeatMs = 0UL;
    if (!runtime.calibrationMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
        setzeLedPins(false, false);
    }
    setzeStateReportOffen();
    logf("INFO", "Setup-Modus beendet (%s)", grund ? grund : "ohne grund");
}

void beendeKalibriermodus(const char* grund, bool messwerteUebernehmen) {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    stoppeFahrt(grund, false);

    if (messwerteUebernehmen &&
        isTravelTimeValid(runtime.candidateTravelTimeUpMs) &&
        isTravelTimeValid(runtime.candidateTravelTimeDownMs)) {
        runtime.travelTimeUpMs = runtime.candidateTravelTimeUpMs;
        runtime.travelTimeDownMs = runtime.candidateTravelTimeDownMs;
        runtime.coverPosition = 100;
    }

    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    runtime.calibrationMode = false;
    runtime.calibrationPhase = CalibrationPhase::Idle;
    berechneKalibrierstatus();
    setzeStateReportOffen();
    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        runtime.calibrationMode = false;
        runtime.calibrationPhase = CalibrationPhase::Idle;
        logf("WARN", "Kalibrierwerte konnten nicht persistiert werden, Vorzustand wiederhergestellt");
    }

    if (!runtime.setupMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
    }

    if (messwerteUebernehmen && runtime.isCalibrated) {
        sendeCoverEvent(SH_EVENT_COVER_CALIB_DONE, SH_TRIGGER_MANUAL_BUTTON, 100U, 0U);
    }
}

void fuehreFactoryResetAus() {
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    stoppeFahrt("factory reset", false);
    runtime.calibrationMode = false;
    runtime.calibrationPhase = CalibrationPhase::Idle;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    nodeProvisioning.applyDefaultBasisValues();
    netZrlProvisioningHandler.loadDeviceDefaults();
    runtime.coverDirection = CoverDirection::None;
    setzeCoverPositionUnbekannt();
    runtime.isCalibrated = false;
    runtime.masterBound = false;
    setzeStateReportOffen();

    if (!nodeProvisioning.clearStoredSettings()) {
        wendeSetupSnapshotAn(previousBasisSnapshot, previousDeviceSnapshot);
        logf("WARN", "Factory Reset konnte Persistenz nicht sauber loeschen, Vorzustand wiederhergestellt");
        return;
    }

    enterSetupMode();
    logf("INFO", "Factory Reset ausgefuehrt");
}

// =============================================================================
// KALIBRIERMODUS – Starten, Messung uebernehmen, Beenden
// =============================================================================

// starteKalibriermodus – Startet Kalibrierungs-State-Machine (faehrt zuerst nach oben)
void starteKalibriermodus() {
    if (runtime.coverState == CoverState::Moving || runtime.setupMode || hatAusstehendeAktion()) return;

    runtime.calibrationMode = true;
    runtime.calibrationPhase = CalibrationPhase::MovingToTop;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    setzeLedMode(LedMode::BothBlink);
    setzeStateReportOffen();
    sendeCoverEvent(SH_EVENT_COVER_CALIB_START, SH_TRIGGER_MANUAL_BUTTON, 0U, 0U);

    if (!starteFahrt(
            CoverDirection::Up,
            "kalibrierung ausgangslage oben",
            runtime.defaultEstimatedTravelTimeMs,
            false)) {
        runtime.calibrationMode = false;
        runtime.calibrationPhase = CalibrationPhase::Idle;
        setzeLedMode(LedMode::Off);
    }
}

void uebernehmeKalibrierMessung(CoverDirection direction) {
    if (!runtime.calibrationMode) return;
    if (runtime.coverState != CoverState::Moving || runtime.coverDirection != direction) return;

    const uint32_t elapsedMs = (uint32_t)(millis() - runtime.movementStartedAtMs);
    if (!isTravelTimeValid(elapsedMs)) {
        beendeKalibriermodus("ungueltige kalibrierzeit", false);
        return;
    }

    if (direction == CoverDirection::Down) {
        runtime.candidateTravelTimeDownMs = elapsedMs;
        stoppeFahrt("kalibrierung down gestoppt", false);
        runtime.calibrationPhase = CalibrationPhase::WaitForUpStart;
        setzeLedMode(LedMode::UpBlink);
    } else {
        runtime.candidateTravelTimeUpMs = elapsedMs;
        stoppeFahrt("kalibrierung up gestoppt", false);
        runtime.calibrationPhase = CalibrationPhase::SuccessBlink;
        runtime.coverPosition = 100;
        runtime.successBlinkToggleCount = CALIBRATION_SUCCESS_BLINK_PULSES * 2U;
        runtime.ledBlinkState = false;
        runtime.ledLastTickMs = millis();
        setzeLedPins(false, false);
    }
}

// =============================================================================
// LED-TICK – Blinken, ACK-Bestaetigung, Pending Actions, Success-Blink
// =============================================================================

// tickLeds – Aktualisiert LED-Zustand (jeder Loop-Aufruf)
void tickLeds() {
    const unsigned long jetztMs = millis();

    if (runtime.ledAckActive) {
        // Delta-Vergleich bleibt auch nach millis()-Wrap korrekt und verhindert haengende ACK-Phasen.
        if (!istZeitfensterAbgelaufen(runtime.ledAckStartedAtMs, LED_ACK_DURATION_MS, jetztMs)) {
            setzeLedPins(true, true);
            return;
        }
        runtime.ledAckActive = false;
        setzeLedMode(runtime.ledModeAfterAck);
    }

    if (hatAusstehendeAktion()) {
        if (runtime.pendingActionBlinkToggleCount == 0U) {
            const PendingAction action = runtime.pendingAction;
            runtime.pendingAction = PendingAction::None;
            runtime.pendingActionBlinkToggleCount = 0U;
            setzeLedPins(false, false);

            if (action == PendingAction::SetupEnter) {
                enterSetupMode();
            } else if (action == PendingAction::FactoryReset) {
                fuehreFactoryResetAus();
            }
            return;
        }

        if ((jetztMs - runtime.ledLastTickMs) >= LED_SUCCESS_INTERVAL_MS) {
            runtime.ledLastTickMs = jetztMs;
            runtime.ledBlinkState = !runtime.ledBlinkState;
            setzeLedPins(runtime.ledBlinkState, runtime.ledBlinkState);
            --runtime.pendingActionBlinkToggleCount;
        }
        return;
    }

    if (runtime.calibrationPhase == CalibrationPhase::SuccessBlink) {
        if (runtime.successBlinkToggleCount == 0U) {
            setzeLedPins(false, false);
            beendeKalibriermodus("kalibrierung erfolgreich", true);
            return;
        }

        if ((jetztMs - runtime.ledLastTickMs) >= LED_SUCCESS_INTERVAL_MS) {
            runtime.ledLastTickMs = jetztMs;
            runtime.ledBlinkState = !runtime.ledBlinkState;
            setzeLedPins(runtime.ledBlinkState, runtime.ledBlinkState);
            --runtime.successBlinkToggleCount;
        }
        return;
    }

    if ((runtime.ledMode == LedMode::BothBlink || runtime.ledMode == LedMode::UpBlink ||
         runtime.ledMode == LedMode::DownBlink) &&
        ((jetztMs - runtime.ledLastTickMs) >= LED_BLINK_INTERVAL_MS)) {
        runtime.ledLastTickMs = jetztMs;
        runtime.ledBlinkState = !runtime.ledBlinkState;
    }

    bool upOn = false;
    bool downOn = false;

    switch (runtime.ledMode) {
        case LedMode::BothBlink:
            upOn = runtime.ledBlinkState;
            downOn = runtime.ledBlinkState;
            break;
        case LedMode::UpBlink:
            upOn = runtime.ledBlinkState;
            break;
        case LedMode::DownBlink:
            downOn = runtime.ledBlinkState;
            break;
        case LedMode::UpOn:
            upOn = true;
            break;
        case LedMode::DownOn:
            downOn = true;
            break;
        case LedMode::Off:
        default:
            break;
    }

    setzeLedPins(upOn, downOn);
}

// =============================================================================
// NORMALE FAHRTEN – Up/Down mit Auto-Stop und EVENTS
// =============================================================================

// starteNormaleFahrtNachOben – Startet Aufwaertsfahrt (Auto-Stop: 12/10 Fahrzeit)
bool starteNormaleFahrtNachOben(const char* grund, uint8_t trigger = SH_TRIGGER_MANUAL_BUTTON) {
    unsigned long autoStopMs = 0UL;
    if (runtime.isCalibrated && isTravelTimeValid(runtime.travelTimeUpMs)) {
        autoStopMs = (runtime.travelTimeUpMs * 12UL) / 10UL;
    } else if (!runtime.isCalibrated && isTravelTimeValid(runtime.defaultEstimatedTravelTimeMs)) {
        autoStopMs = runtime.defaultEstimatedTravelTimeMs;
    }

    const bool gestartet =
        starteFahrt(CoverDirection::Up, grund, autoStopMs, autoStopMs > 0UL);
    if (gestartet && !runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(LedMode::UpOn);
        sendeCoverEvent(SH_EVENT_COVER_UP, trigger, coverPositionFuerPayload(), 100U);
    }
    return gestartet;
}

bool starteNormaleFahrtNachUnten(const char* grund, uint8_t trigger = SH_TRIGGER_MANUAL_BUTTON) {
    unsigned long autoStopMs = 0UL;
    if (runtime.isCalibrated && isTravelTimeValid(runtime.travelTimeDownMs)) {
        autoStopMs = (runtime.travelTimeDownMs * 12UL) / 10UL;
    } else if (!runtime.isCalibrated && isTravelTimeValid(runtime.defaultEstimatedTravelTimeMs)) {
        autoStopMs = runtime.defaultEstimatedTravelTimeMs;
    }

    const bool gestartet =
        starteFahrt(CoverDirection::Down, grund, autoStopMs, autoStopMs > 0UL);
    if (gestartet && !runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(LedMode::DownOn);
        sendeCoverEvent(SH_EVENT_COVER_DOWN, trigger, coverPositionFuerPayload(), 0U);
    }
    return gestartet;
}

bool starteEndlagenfahrtOhneKalibrierung(int16_t zielPosition, const char* grund, uint8_t trigger) {
    const int16_t gekapptesZiel = begrenzePosition(zielPosition);
    if (gekapptesZiel == 100) {
        // Ohne Kalibrierung ist 100 keine echte Zielposition, sondern nur die volle Auf-Fahrt.
        return starteNormaleFahrtNachOben(grund, trigger);
    }
    if (gekapptesZiel == 0) {
        // Zwischenwerte bleiben ohne Kalibrierung gesperrt; 0 steht nur fuer die volle Ab-Fahrt.
        return starteNormaleFahrtNachUnten(grund, trigger);
    }
    return false;
}

// =============================================================================
// SERIELLE KONSOLE – parseUInt, Status, Hilfe, Setup-Befehle, Befehlsparser
// =============================================================================

// parseUIntValue – Parst String in uint32_t
bool parseUIntValue(const char* text, uint32_t& outValue) {
    if (text == nullptr || *text == '\0') return false;

    char* endPtr = nullptr;
    const unsigned long parsed = strtoul(text, &endPtr, 10);
    if (endPtr == text || *endPtr != '\0') return false;

    outValue = (uint32_t)parsed;
    return true;
}

void gibStatusAus() {
    char masterMacText[MASTER_MAC_TEXT_LEN] = {0};
    formatMacText(runtime.masterMac, runtime.masterMacValid, masterMacText, sizeof(masterMacText));
    char coverPositionText[16] = {0};
    if (istCoverPositionBekannt(runtime.coverPosition)) {
        snprintf(coverPositionText, sizeof(coverPositionText), "%d", runtime.coverPosition);
    } else {
        copyText(coverPositionText, sizeof(coverPositionText), "unknown");
    }

    char buffer[560];
    snprintf(
        buffer,
        sizeof(buffer),
        "state=%s direction=%s relay_a=%s relay_b=%s calibration_mode=%s calibration_phase=%s setup_mode=%s setup_ap=%s setup_ssid=%s pending_action=%s pending_blinks=%u is_calibrated=%s master_mac=%s travel_time_up_ms=%lu travel_time_down_ms=%lu default_estimated_travel_time_ms=%lu relay_up_mapping=%s status_send_interval_s=%lu sensor_send_interval_s=%lu cover_position=%s",
        toText(runtime.coverState),
        toText(runtime.coverDirection),
        runtime.relayAActive ? "true" : "false",
        runtime.relayBActive ? "true" : "false",
        runtime.calibrationMode ? "true" : "false",
        toText(runtime.calibrationPhase),
        runtime.setupMode ? "true" : "false",
        runtime.setupApActive ? "true" : "false",
        runtime.setupApActive ? runtime.setupApSsid : "-",
        toText(runtime.pendingAction),
        runtime.pendingActionBlinkToggleCount,
        runtime.isCalibrated ? "true" : "false",
        runtime.masterMacValid ? masterMacText : "unset",
        (unsigned long)runtime.travelTimeUpMs,
        (unsigned long)runtime.travelTimeDownMs,
        (unsigned long)runtime.defaultEstimatedTravelTimeMs,
        runtime.relayUpUsesRelayA ? "relay_a" : "relay_b",
        (unsigned long)runtime.statusSendIntervalS,
        (unsigned long)runtime.sensorSendIntervalS,
        coverPositionText);
    Serial.println(buffer);
}

void gibHilfeAus() {
    Serial.println("Befehle:");
    Serial.println("  status");
    Serial.println("  up");
    Serial.println("  down");
    Serial.println("  stop");
    Serial.println("  cal abort");
    Serial.println("  setup show");
    Serial.println("  setup enter");
    Serial.println("  setup exit");
    Serial.println("  setup set master_mac AA:BB:CC:DD:EE:FF");
    Serial.println("  setup set tt_up_ms <ms>");
    Serial.println("  setup set tt_down_ms <ms>");
    Serial.println("  setup set default_ms <ms>");
    Serial.println("  setup set relay_up relay_a|relay_b");
    Serial.println("  setup set status_interval_s <s>");
    Serial.println("  setup set sensor_interval_s <s>");
    Serial.println("  setup reset_calibration");
    Serial.println("  help");
}

void verarbeiteSetupSet(const char* commandPrefix, const char* valueText) {
    if (!runtime.setupMode) {
        Serial.println("Setup-Modus ist nicht aktiv.");
        return;
    }

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    uint32_t value = 0UL;
    if (strcmp(commandPrefix, "setup set relay_up ") == 0) {
        if (strcmp(valueText, "relay_a") == 0) runtime.relayUpUsesRelayA = true;
        else if (strcmp(valueText, "relay_b") == 0) runtime.relayUpUsesRelayA = false;
        else {
            Serial.println("Ungueltige Relaiszuordnung.");
            return;
        }

        if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
            Serial.println("Speichern fehlgeschlagen, Vorzustand wiederhergestellt.");
            return;
        }
        Serial.println("Relaiszuordnung gespeichert.");
        return;
    }

    if (strcmp(commandPrefix, "setup set master_mac ") == 0) {
        uint8_t parsedMac[6] = {0};
        if (!parseMacText(valueText, parsedMac)) {
            Serial.println("Ungueltige Master-MAC.");
            return;
        }
        setStoredMasterMac(parsedMac);
        runtime.masterBound = false;
        if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
            Serial.println("Speichern fehlgeschlagen, Vorzustand wiederhergestellt.");
            return;
        }
        Serial.println("Master-MAC gespeichert.");
        return;
    }

    if (!parseUIntValue(valueText, value)) {
        Serial.println("Ungueltiger Zahlenwert.");
        return;
    }

    if (strcmp(commandPrefix, "setup set tt_up_ms ") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("tt_up_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.travelTimeUpMs = value;
    } else if (strcmp(commandPrefix, "setup set tt_down_ms ") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("tt_down_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.travelTimeDownMs = value;
    } else if (strcmp(commandPrefix, "setup set default_ms ") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("default_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(value);
    } else if (strcmp(commandPrefix, "setup set status_interval_s ") == 0) {
        if (!isSendIntervalValid(value)) {
            Serial.println("status_interval_s ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.statusSendIntervalS = value;
    } else if (strcmp(commandPrefix, "setup set sensor_interval_s ") == 0) {
        if (!isSendIntervalValid(value)) {
            Serial.println("sensor_interval_s ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.sensorSendIntervalS = value;
    } else {
        Serial.println("Unbekannte Setup-Option.");
        return;
    }

    berechneKalibrierstatus();
    setzeStateReportOffen();
    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        Serial.println("Speichern fehlgeschlagen, Vorzustand wiederhergestellt.");
        return;
    }
    Serial.println("Setup-Wert gespeichert.");
}

void verarbeiteBefehl(char* line) {
    if (line == nullptr) return;

    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return;

    if (strcmp(line, "status") == 0) {
        gibStatusAus();
        return;
    }
    if (strcmp(line, "help") == 0) {
        gibHilfeAus();
        return;
    }
    if (strcmp(line, "stop") == 0) {
        if (resetBestaetigungAktiv()) {
            verwerfeAusstehendeAktion("serieller reset-abbruch");
        } else if (runtime.calibrationMode &&
                   (runtime.calibrationPhase == CalibrationPhase::MeasuringDown ||
                    runtime.calibrationPhase == CalibrationPhase::MeasuringUp)) {
            uebernehmeKalibrierMessung(runtime.coverDirection);
        } else if (runtime.calibrationMode) {
            beendeKalibriermodus("serieller kalibrierabbruch", false);
        } else {
            stoppeFahrtMitEvent("serieller stop", SH_TRIGGER_CONFIG);
            setzeLedNachNormalemStop();
        }
        return;
    }
    if (strcmp(line, "up") == 0) {
        if (runtime.calibrationMode && runtime.calibrationPhase == CalibrationPhase::WaitForUpStart) {
            bestaetigeMitBeidenLeds(LedMode::UpOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringUp;
            starteFahrt(CoverDirection::Up, "kalibrierung messfahrt up", MAX_TRAVEL_TIME_MS, false);
            return;
        }
        starteNormaleFahrtNachOben("serieller up", SH_TRIGGER_CONFIG);
        return;
    }
    if (strcmp(line, "down") == 0) {
        if (runtime.calibrationMode && runtime.calibrationPhase == CalibrationPhase::WaitForDownStart) {
            bestaetigeMitBeidenLeds(LedMode::DownOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringDown;
            starteFahrt(CoverDirection::Down, "kalibrierung messfahrt down", MAX_TRAVEL_TIME_MS, false);
            return;
        }
        starteNormaleFahrtNachUnten("serieller down", SH_TRIGGER_CONFIG);
        return;
    }
    if (strcmp(line, "cal abort") == 0) {
        beendeKalibriermodus("serieller kalibrierabbruch", false);
        return;
    }
    if (strcmp(line, "setup show") == 0) {
        gibStatusAus();
        return;
    }
    if (strcmp(line, "setup enter") == 0) {
        enterSetupMode();
        return;
    }
    if (strcmp(line, "setup exit") == 0) {
        exitSetupMode("seriell");
        return;
    }
    if (strcmp(line, "setup reset_calibration") == 0) {
        if (!runtime.setupMode) {
            Serial.println("Setup-Modus ist nicht aktiv.");
            return;
        }
        setzeKalibrierungUngueltig();
        Serial.println("Kalibrierung geloescht.");
        return;
    }

    const char* setupCommands[] = {
        "setup set master_mac ",
        "setup set tt_up_ms ",
        "setup set tt_down_ms ",
        "setup set default_ms ",
        "setup set relay_up ",
        "setup set status_interval_s ",
        "setup set sensor_interval_s "
    };

    for (size_t index = 0; index < (sizeof(setupCommands) / sizeof(setupCommands[0])); ++index) {
        const char* prefix = setupCommands[index];
        const size_t prefixLen = strlen(prefix);
        if (strncmp(line, prefix, prefixLen) == 0) {
            verarbeiteSetupSet(prefix, line + prefixLen);
            return;
        }
    }

    Serial.print("Unbekannter Befehl: ");
    Serial.println(line);
    gibHilfeAus();
}

void verarbeiteSerielleBefehle() {
    while (Serial.available() > 0) {
        const char ch = (char)Serial.read();
        if (ch == '\r') continue;

        if (ch == '\n') {
            runtime.serialBuffer[runtime.serialLength] = '\0';
            verarbeiteBefehl(runtime.serialBuffer);
            runtime.serialLength = 0U;
            runtime.serialBuffer[0] = '\0';
            continue;
        }

        if (runtime.serialLength + 1U >= SERIAL_BUFFER_SIZE) {
            runtime.serialLength = 0U;
            runtime.serialBuffer[0] = '\0';
            logf("WARN", "serieller Befehl zu lang, Eingabe verworfen");
            continue;
        }

        runtime.serialBuffer[runtime.serialLength++] = ch;
        runtime.serialBuffer[runtime.serialLength] = '\0';
    }
}

// =============================================================================
// BEWEGUNGS-TIMEOUTS – Auto-Stop erreicht, Kalibrierungs-Timeouts
// =============================================================================

// verarbeiteBewegungsTimeouts – Prueft ob aktive Fahrt Auto-Stop erreicht hat
void verarbeiteBewegungsTimeouts() {
    if (runtime.coverState != CoverState::Moving) return;
    if (runtime.movementAutoStopMs == 0UL) return;

    const unsigned long jetztMs = millis();
    // Delta-Logik ist fuer Langzeitbetrieb noetig, weil Absolut-Deadlines am millis()-Wrap kippen.
    if (!istZeitfensterAbgelaufen(runtime.movementStartedAtMs, runtime.movementAutoStopMs, jetztMs)) return;

    if (runtime.calibrationMode && runtime.calibrationPhase == CalibrationPhase::MovingToTop) {
        stoppeFahrt("kalibrierung ausgangslage erreicht", false);
        runtime.calibrationPhase = CalibrationPhase::WaitForDownStart;
        setzeLedMode(LedMode::DownBlink);
        return;
    }

    if (runtime.calibrationMode &&
        (runtime.calibrationPhase == CalibrationPhase::MeasuringDown ||
         runtime.calibrationPhase == CalibrationPhase::MeasuringUp)) {
        beendeKalibriermodus("kalibrierung timeout", false);
        return;
    }

    if (runtime.movementTargetsIntermediatePosition) {
        runtime.coverPosition = begrenzePosition(runtime.movementTargetPosition);
    }
    if (runtime.movementTargetsEndPosition) {
        setzePositionAufEndlage(runtime.coverDirection);
    }
    // Die grobe 0/100-Anzeige ist ohne Kalibrierung nur nach normalem Fahrtende belastbar.
    stoppeFahrt("fahrzeit erreicht", runtime.movementTargetsEndPosition);
    setzeLedNachNormalemStop();
}

// =============================================================================
// TASTER-HANDLER – Up/Down/Stop mit Hold-Gesten (5s)
//   Stop+5s=Kalibrierung, Up+5s=FactoryReset, Down+5s=Setup
// =============================================================================

// behandleStopButton – Stop-Taster: kurz=Stop, lang(5s)=Kalibrierung starten
void behandleStopButton(bool stopActive, unsigned long jetztMs) {
    if (stopActive && !runtime.lastStopButtonActive) {
        loggeButtonKante("stop", true, 0UL);
        runtime.stopPressedAtMs = jetztMs;
        runtime.stopHoldConsumed = false;

        if (resetBestaetigungAktiv()) {
            verwerfeAusstehendeAktion("reset per stop abgebrochen");
            runtime.stopHoldConsumed = true;
        } else if (runtime.coverState == CoverState::Moving) {
            if (runtime.calibrationMode &&
                (runtime.calibrationPhase == CalibrationPhase::MeasuringDown ||
                 runtime.calibrationPhase == CalibrationPhase::MeasuringUp)) {
                uebernehmeKalibrierMessung(runtime.coverDirection);
            } else {
                stoppeFahrtMitEvent("lokaler taster stop", SH_TRIGGER_MANUAL_BUTTON);
                setzeLedNachNormalemStop();
            }
            runtime.stopHoldConsumed = true;
        } else if (runtime.calibrationMode) {
            beendeKalibriermodus("lokaler kalibrierabbruch", false);
            runtime.stopHoldConsumed = true;
        } else if (runtime.setupMode) {
            exitSetupMode("lokaler stop");
            runtime.stopHoldConsumed = true;
        }
    }

    if (stopActive && !runtime.stopHoldConsumed &&
        runtime.coverState == CoverState::Stopped &&
        !runtime.calibrationMode && !runtime.setupMode && !hatAusstehendeAktion() &&
        runtime.stopPressedAtMs > 0UL &&
        (jetztMs - runtime.stopPressedAtMs) >= HOLD_MS) {
        runtime.stopHoldConsumed = true;
        starteKalibriermodus();
    }

    if (!stopActive && runtime.lastStopButtonActive) {
        const unsigned long heldMs =
            runtime.stopPressedAtMs > 0UL ? (jetztMs - runtime.stopPressedAtMs) : 0UL;
        loggeButtonKante("stop", false, heldMs);
        runtime.stopPressedAtMs = 0UL;
        runtime.stopHoldConsumed = false;
    }

    runtime.lastStopButtonActive = stopActive;
}

void behandleUpButton(bool upActive, unsigned long jetztMs) {
    if (upActive && !runtime.lastUpButtonActive) {
        loggeButtonKante("up", true, 0UL);
        runtime.upPressedAtMs = jetztMs;
        runtime.upHoldConsumed = false;

        if (runtime.coverState == CoverState::Moving) {
            logf("INFO", "Up ignoriert: waehrend Fahrt ist nur stop relevant");
        } else if (runtime.setupMode) {
            logf("INFO", "Up ignoriert: Setup-Modus aktiv");
        } else if (runtime.calibrationMode &&
                   runtime.calibrationPhase == CalibrationPhase::WaitForUpStart) {
            bestaetigeMitBeidenLeds(LedMode::UpOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringUp;
            starteFahrt(CoverDirection::Up, "kalibrierung messfahrt up", MAX_TRAVEL_TIME_MS, false);
            runtime.upHoldConsumed = true;
        }
    }

    if (upActive && !runtime.upHoldConsumed &&
        !runtime.calibrationMode && !runtime.setupMode &&
        runtime.coverState == CoverState::Stopped &&
        runtime.upPressedAtMs > 0UL && !hatAusstehendeAktion() &&
        (jetztMs - runtime.upPressedAtMs) >= HOLD_MS) {
        runtime.upHoldConsumed = true;
        starteAusstehendeAktion(PendingAction::FactoryReset, RESET_CONFIRM_BLINK_PULSES);
    }

    if (!upActive && runtime.lastUpButtonActive) {
        const unsigned long heldMs =
            runtime.upPressedAtMs > 0UL ? (jetztMs - runtime.upPressedAtMs) : 0UL;
        loggeButtonKante("up", false, heldMs);

        if (!runtime.upHoldConsumed &&
            !runtime.calibrationMode && !runtime.setupMode &&
            runtime.coverState == CoverState::Stopped &&
            !hatAusstehendeAktion() && heldMs > 0UL) {
            starteNormaleFahrtNachOben("lokaler taster up");
        }

        runtime.upPressedAtMs = 0UL;
        runtime.upHoldConsumed = false;

        if (!runtime.setupMode && !runtime.calibrationMode &&
            runtime.coverState == CoverState::Stopped && !hatAusstehendeAktion()) {
            setzeLedMode(LedMode::Off);
        }
    }

    runtime.lastUpButtonActive = upActive;
}

void behandleDownButton(bool downActive, unsigned long jetztMs) {
    if (downActive && !runtime.lastDownButtonActive) {
        loggeButtonKante("down", true, 0UL);
        runtime.downPressedAtMs = jetztMs;
        runtime.downHoldConsumed = false;

        if (runtime.coverState == CoverState::Moving) {
            logf("INFO",
                 "Down ignoriert: waehrend Fahrt ist nur stop relevant (state=%s direction=%s setup=%s calibration=%s)",
                 toText(runtime.coverState),
                 toText(runtime.coverDirection),
                 runtime.setupMode ? "true" : "false",
                 runtime.calibrationMode ? "true" : "false");
        } else if (runtime.calibrationMode &&
                   runtime.calibrationPhase == CalibrationPhase::WaitForDownStart) {
            bestaetigeMitBeidenLeds(LedMode::DownOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringDown;
            starteFahrt(CoverDirection::Down, "kalibrierung messfahrt down", MAX_TRAVEL_TIME_MS, false);
            runtime.downHoldConsumed = true;
        }
    }

    if (downActive && !runtime.calibrationMode && !runtime.setupMode &&
        runtime.coverState == CoverState::Stopped &&
        runtime.downPressedAtMs > 0UL && !runtime.downHoldConsumed &&
        !hatAusstehendeAktion() &&
        (jetztMs - runtime.downPressedAtMs) >= HOLD_MS) {
        runtime.downHoldConsumed = true;
        starteAusstehendeAktion(PendingAction::SetupEnter, SETUP_CONFIRM_BLINK_PULSES);
    }

    if (!downActive && runtime.lastDownButtonActive) {
        const unsigned long heldMs =
            runtime.downPressedAtMs > 0UL ? (jetztMs - runtime.downPressedAtMs) : 0UL;
        loggeButtonKante("down", false, heldMs);

        if (!runtime.calibrationMode && !runtime.setupMode &&
            runtime.coverState == CoverState::Stopped &&
            !runtime.downHoldConsumed && !hatAusstehendeAktion() &&
            heldMs > 0UL) {
            starteNormaleFahrtNachUnten("lokaler taster down");
        }

        runtime.downPressedAtMs = 0UL;
        runtime.downHoldConsumed = false;

        if (!runtime.setupMode && !runtime.calibrationMode &&
            runtime.coverState == CoverState::Stopped && !hatAusstehendeAktion()) {
            setzeLedMode(LedMode::Off);
        }
    }

    runtime.lastDownButtonActive = downActive;
}

void pollButtons() {
    const unsigned long jetztMs = millis();
    if ((jetztMs - runtime.lastButtonPollMs) < BUTTON_POLL_MS) return;
    runtime.lastButtonPollMs = jetztMs;

    runtime.upButtonStableActive = entprelleButton(
        leseButtonAktiv(PIN_BUTTON_UP),
        runtime.upButtonRawActive,
        runtime.upButtonRawChangedAtMs,
        runtime.upButtonStableActive,
        jetztMs);

    runtime.downButtonStableActive = entprelleButton(
        leseButtonAktiv(PIN_BUTTON_DOWN),
        runtime.downButtonRawActive,
        runtime.downButtonRawChangedAtMs,
        runtime.downButtonStableActive,
        jetztMs);

    runtime.stopButtonStableActive = entprelleButton(
        leseButtonAktiv(PIN_BUTTON_STOP),
        runtime.stopButtonRawActive,
        runtime.stopButtonRawChangedAtMs,
        runtime.stopButtonStableActive,
        jetztMs);

    behandleStopButton(runtime.stopButtonStableActive, jetztMs);
    behandleUpButton(runtime.upButtonStableActive, jetztMs);
    behandleDownButton(runtime.downButtonStableActive, jetztMs);
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG – HELLO_ACK, CMD (Cover-Kommandos), CFG
// =============================================================================

// uebernehmeReportIntervalAusCfg – Wendet neues Report-Intervall aus CFG an
bool uebernehmeReportIntervalAusCfg(uint16_t value) {
    if (!isSendIntervalValid(value)) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    runtime.statusSendIntervalS = value;
    setzeStateReportOffen();
    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        logf("WARN", "report_interval_s konnte nicht persistiert werden");
        return false;
    }

    return true;
}

void merkeCmdAck(uint8_t seq, uint8_t status) {
    runtime.lastCmdAckValid = true;
    runtime.lastCmdSeq = seq;
    runtime.lastCmdAckStatus = status;
}

void merkeCfgAck(uint8_t seq, uint8_t status) {
    runtime.lastCfgAckValid = true;
    runtime.lastCfgSeq = seq;
    runtime.lastCfgAckStatus = status;
}

void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    if (!uebernehmeMasterMacNachHelloAck(senderMac)) {
        logf("WARN", "Master-MAC aus HELLO_ACK konnte nicht uebernommen werden");
        return;
    }

    stellePeerSicher(runtime.masterMac);
    setzeStateReportOffen();
    sendeState();
    logf("INFO", "HELLO_ACK empfangen");
}

void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CmdPayload& payload) {
    if (!senderIstBekannterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der bekannte Master");
        return;
    }

    if ((header.flags & SH_FLAG_ACK_REQUEST) &&
        runtime.lastCmdAckValid &&
        header.seq == runtime.lastCmdSeq) {
        sendeAck(senderMac, header.seq, header.msg_type, runtime.lastCmdAckStatus);
        return;
    }

    uint8_t ackStatus = SH_ACK_REJECTED;

    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        setzeStateReportOffen();
        sendeState();
        ackStatus = SH_ACK_OK;
    } else if (payload.cmd_type == SH_CMD_COVER) {
        if (runtime.setupMode || runtime.calibrationMode || hatAusstehendeAktion()) {
            ackStatus = SH_ACK_REJECTED;
        } else {
            switch (payload.param1) {
                case SH_COVER_CMD_OPEN:
                    ackStatus = starteNormaleFahrtNachOben("master open", SH_TRIGGER_MASTER_CMD)
                                    ? SH_ACK_OK
                                    : SH_ACK_REJECTED;
                    break;

                case SH_COVER_CMD_CLOSE:
                    ackStatus = starteNormaleFahrtNachUnten("master close", SH_TRIGGER_MASTER_CMD)
                                    ? SH_ACK_OK
                                    : SH_ACK_REJECTED;
                    break;

                case SH_COVER_CMD_STOP:
                    stoppeFahrtMitEvent("master stop", SH_TRIGGER_MASTER_CMD);
                    setzeLedNachNormalemStop();
                    ackStatus = SH_ACK_OK;
                    break;

                case SH_COVER_CMD_SET_POSITION:
                    if (!runtime.isCalibrated) {
                        ackStatus =
                            starteEndlagenfahrtOhneKalibrierung((int16_t)payload.param2, "master set_position", SH_TRIGGER_MASTER_CMD)
                                ? SH_ACK_OK
                                : SH_ACK_REJECTED;
                    } else if (startePositionsfahrt((int16_t)payload.param2, "master set_position")) {
                        if (runtime.coverState == CoverState::Moving) {
                            sendeCoverEvent(
                                runtime.coverDirection == CoverDirection::Up ? SH_EVENT_COVER_UP : SH_EVENT_COVER_DOWN,
                                SH_TRIGGER_MASTER_CMD,
                                coverPositionFuerPayload(),
                                (uint16_t)payload.param2);
                        }
                        ackStatus = SH_ACK_OK;
                    } else {
                        ackStatus = SH_ACK_REJECTED;
                    }
                    break;

                default:
                    ackStatus = SH_ACK_REJECTED;
                    break;
            }
        }
    }

    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ackStatus);
        merkeCmdAck(header.seq, ackStatus);
    }
}

void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    if (!senderIstBekannterMaster(senderMac)) {
        logf("WARN", "CFG ignoriert: Sender ist nicht der bekannte Master");
        return;
    }

    if ((header.flags & SH_FLAG_ACK_REQUEST) &&
        runtime.lastCfgAckValid &&
        header.seq == runtime.lastCfgSeq) {
        sendeAck(senderMac, header.seq, header.msg_type, runtime.lastCfgAckStatus);
        return;
    }

    uint8_t ackStatus = SH_ACK_ERROR;
    if (payload.param_id == SH_CFG_REPORT_INTERVAL_S && uebernehmeReportIntervalAusCfg(payload.value)) {
        ackStatus = SH_ACK_OK;
        sendeState();
    } else {
        ackStatus = SH_ACK_REJECTED;
    }

    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ackStatus);
        merkeCfgAck(header.seq, ackStatus);
    }
}

// =============================================================================
// ESP-NOW – Paket-Empfang, Callbacks, Funk-Initialisierung
// =============================================================================

// verarbeiteEspNowPaket – Validiert CRC und leitet an Handler weiter (switch/msg_type)
void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* data, int len) {
    if (!senderMac || !data || len < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(data, (size_t)len)) {
        logf("WARN", "ESP-NOW Paket verworfen: CRC/Header ungueltig");
        return;
    }

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

void onEspNowSend(const wifi_tx_info_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

void initialisiereFunk() {
    if (runtime.funkBereit || runtime.setupMode) return;
    static uint8_t espNowInitFails = 0;
    constexpr uint8_t MAX_ESPNOW_INIT_FAILURES = 5;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);

    const esp_err_t kanalErr = esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (kanalErr != ESP_OK) {
        logf("WARN", "WLAN-Kanal %d konnte nicht gesetzt werden (err=%d)", WLAN_KANAL, (int)kanalErr);
    }

    if (esp_now_init() != ESP_OK) {
        espNowInitFails++;
        logf("WARN", "ESP-NOW Initialisierung fehlgeschlagen (%u/%u)", espNowInitFails, MAX_ESPNOW_INIT_FAILURES);
        if (espNowInitFails >= MAX_ESPNOW_INIT_FAILURES) {
            logf("ERROR", "ESP-NOW init nach %u Versuchen fehlgeschlagen, restart", MAX_ESPNOW_INIT_FAILURES);
            ESP.restart();
        }
        return;
    }
    espNowInitFails = 0;  // Reset on success

    esp_now_register_send_cb(onEspNowSend);
    esp_now_register_recv_cb(onEspNowReceive);
    stellePeerSicher(BROADCAST_MAC);
    if (runtime.masterMacValid) {
        stellePeerSicher(runtime.masterMac);
    }
    runtime.funkBereit = true;
}

void tickKommunikation() {
    if (runtime.setupMode) return;

    if (!runtime.funkBereit) {
        initialisiereFunk();
        if (!runtime.funkBereit) return;
    }

    const unsigned long jetztMs = millis();
    if (!runtime.masterBound) {
        if (runtime.letztesHelloMs == 0UL || (jetztMs - runtime.letztesHelloMs) >= HELLO_RETRY_INTERVAL_MS) {
            sendeHello();
        }
        return;
    }

    if (runtime.letztesHelloMs == 0UL || (jetztMs - runtime.letztesHelloMs) >= HELLO_REANNOUNCE_INTERVAL_MS) {
        sendeHello();
    }

    if (runtime.letzterHeartbeatMs == 0UL || (jetztMs - runtime.letzterHeartbeatMs) >= HEARTBEAT_INTERVAL_MS) {
        sendeHeartbeat();
    }

    const unsigned long reportInterval = stateIntervalMs();
    if (runtime.stateReportOffen ||
        runtime.letzterStateMs == 0UL ||
        (reportInterval > 0UL && (jetztMs - runtime.letzterStateMs) >= reportInterval)) {
        sendeState();
    }
}

void initialisierePin(int pin, uint8_t mode) {
    if (pin >= 0) pinMode(pin, mode);
}

}  // namespace

// =============================================================================
// ARDUINO ENTRY POINTS – setup() und loop() (globaler Scope)
// =============================================================================

void setup() {
    // Serial bleibt hier bewusst aktiv: NET-ZRL nutzt die Konsole nicht nur fuer Debug,
    // sondern auch fuer lokale Setup- und Kalibrierbefehle.
    Serial.begin(115200);
    delay(150);

    runtime = {};
    runtime.coverState = CoverState::Stopped;
    runtime.coverDirection = CoverDirection::None;
    runtime.coverPosition = COVER_POSITION_UNBEKANNT;
    runtime.calibrationPhase = CalibrationPhase::Idle;
    runtime.ledMode = LedMode::Off;
    runtime.ledModeAfterAck = LedMode::Off;
    runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
    runtime.relayUpUsesRelayA = true;
    runtime.statusSendIntervalS = DEFAULT_STATUS_SEND_INTERVAL_S;
    runtime.sensorSendIntervalS = DEFAULT_SENSOR_SEND_INTERVAL_S;
    runtime.stateReportOffen = true;
    runtime.movementTargetPosition = 0;

    initialisierePin(PIN_RELAY_A, OUTPUT);
    initialisierePin(PIN_RELAY_B, OUTPUT);
    initialisierePin(PIN_LED_UP, OUTPUT);
    initialisierePin(PIN_LED_DOWN, OUTPUT);

    setzeLedPins(false, false);
    setzeRelaisNeutral("boot");
    runtime.letzteRelaisWechselMs = 0;

    // Watchdog initialisieren
    esp_task_wdt_deinit();
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = (uint32_t)NET_ZRL_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    logf("INFO", "Watchdog aktiviert (%d s)", NET_ZRL_WDT_TIMEOUT_S);

    initialisierePin(PIN_BUTTON_UP, INPUT_PULLUP);
    initialisierePin(PIN_BUTTON_DOWN, INPUT_PULLUP);
    initialisierePin(PIN_BUTTON_STOP, INPUT_PULLUP);

    const SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig =
        SmartHome::NetZrlProvisioning::makeConfig(
            DEVICE_ID,
            DEFAULT_STATUS_SEND_INTERVAL_S,
            DEFAULT_SENSOR_SEND_INTERVAL_S,
            MIN_SEND_INTERVAL_S,
            MAX_SEND_INTERVAL_S);

    if (!nodeProvisioning.begin(
            provisioningConfig,
            &runtime.masterMacValid,
            runtime.masterMac,
            &runtime.statusSendIntervalS,
            &runtime.sensorSendIntervalS,
            &runtime.setupMode,
            &runtime.setupApActive,
            &runtime.restartPending,
            &runtime.restartRequestedAtMs,
            runtime.setupApSsid,
            sizeof(runtime.setupApSsid),
            &netZrlProvisioningHandler,
            provisioningLog)) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
    }

    runtime.statusSendIntervalS = sanitizeStatusSendInterval(runtime.statusSendIntervalS);
    runtime.sensorSendIntervalS = sanitizeSensorSendInterval(runtime.sensorSendIntervalS);
    runtime.masterBound = false;
    runtime.funkBereit = false;
    runtime.naechsteSeq = 1U;

    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        enterSetupMode();
        return;
    }

    initialisiereFunk();

    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO",
         "Pins relay_a=%d relay_b=%d button_up=%d button_down=%d button_stop=%d led_up=%d led_down=%d button_active_low=%s",
         PIN_RELAY_A,
         PIN_RELAY_B,
         PIN_BUTTON_UP,
         PIN_BUTTON_DOWN,
         PIN_BUTTON_STOP,
         PIN_LED_UP,
         PIN_LED_DOWN,
         BUTTON_ACTIVE_LOW ? "true" : "false");

    gibHilfeAus();
    gibStatusAus();
}

void loop() {
    esp_task_wdt_reset();
    verarbeiteSerielleBefehle();
    pollButtons();
    verarbeiteBewegungsTimeouts();

    if (runtime.coverState == CoverState::Moving && runtime.isCalibrated) {
        aktualisierePositionsschaetzung(millis());
    }

    nodeProvisioning.update();
    tickKommunikation();
    tickLeds();
    delay(LOOP_DELAY_MS);
}
