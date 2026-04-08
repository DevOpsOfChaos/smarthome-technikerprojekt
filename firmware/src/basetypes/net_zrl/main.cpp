
#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

namespace {

constexpr char DATEI_GERAET[] = "NET-ZRL";
constexpr char DATEI_VERSION[] = "0.4.0";
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
constexpr size_t SERIAL_BUFFER_SIZE = 128U;
constexpr size_t MASTER_MAC_TEXT_LEN = SmartHome::ShNodeProvisioning::MASTER_MAC_TEXT_LEN;
constexpr size_t SETUP_SSID_BUFFER_SIZE = 32U;
constexpr const char* STORAGE_NAMESPACE = "net_zrl";
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";
constexpr const char* STORAGE_KEY_NET_ZRL_BLOB = "net_zrl_v1";
constexpr const char* SETUP_AP_PREFIX = "NET-ZRL-SETUP";
constexpr int SETUP_AP_CHANNEL = 1;
constexpr uint32_t NET_ZRL_SETUP_MAGIC = 0x5A524C32UL;
constexpr uint16_t NET_ZRL_SETUP_VERSION = 1U;

enum class CoverState : uint8_t { Stopped = 0, Moving = 1 };
enum class CoverDirection : uint8_t { None = 0, Up = 1, Down = 2 };
enum class CalibrationPhase : uint8_t {
    Idle = 0,
    MovingToTop,
    WaitForDownStart,
    MeasuringDown,
    WaitForUpStart,
    MeasuringUp,
    SuccessBlink
};
enum class LedMode : uint8_t { Off = 0, BothBlink, UpBlink, DownBlink, UpOn, DownOn };
enum class PendingAction : uint8_t { None = 0, SetupEnter, FactoryReset };

struct NetZrlPersistedSetupData {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    uint8_t relayUpUsesRelayA;
    uint8_t reservedBytes[3];
};

struct NetZrlSetupSnapshot {
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    bool relayUpUsesRelayA;
};

struct RuntimeState {
    CoverState coverState;
    CoverDirection coverDirection;
    bool relayAActive;
    bool relayBActive;
    bool relayUpUsesRelayA;
    bool masterMacValid;
    uint8_t masterMac[6];
    bool calibrationMode;
    bool setupMode;
    bool isCalibrated;
    bool setupApActive;
    bool restartPending;
    bool funkBereit;
    bool masterBound;
    bool stateReportOffen;
    bool movementTargetsIntermediatePosition;
    int16_t coverPosition;
    int16_t movementTargetPosition;
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t candidateTravelTimeUpMs;
    uint32_t candidateTravelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
    CalibrationPhase calibrationPhase;
    unsigned long movementStartedAtMs;
    unsigned long movementDeadlineAtMs;
    unsigned long restartRequestedAtMs;
    unsigned long letztesHelloMs;
    unsigned long letzterHeartbeatMs;
    unsigned long letzterStateMs;
    int16_t movementStartPosition;
    bool movementTargetsEndPosition;
    uint8_t naechsteSeq;
    bool lastCmdAckValid;
    bool lastCfgAckValid;
    uint8_t lastCmdSeq;
    uint8_t lastCfgSeq;
    uint8_t lastCmdAckStatus;
    uint8_t lastCfgAckStatus;

    unsigned long lastButtonPollMs;
    bool upButtonStableActive;
    bool downButtonStableActive;
    bool stopButtonStableActive;
    bool upButtonRawActive;
    bool downButtonRawActive;
    bool stopButtonRawActive;
    unsigned long upButtonRawChangedAtMs;
    unsigned long downButtonRawChangedAtMs;
    unsigned long stopButtonRawChangedAtMs;
    bool lastUpButtonActive;
    bool lastDownButtonActive;
    bool lastStopButtonActive;
    unsigned long upPressedAtMs;
    unsigned long downPressedAtMs;
    unsigned long stopPressedAtMs;
    bool upHoldConsumed;
    bool downHoldConsumed;
    bool stopHoldConsumed;

    LedMode ledMode;
    LedMode ledModeAfterAck;
    bool ledBlinkState;
    unsigned long ledLastTickMs;
    unsigned long ledAckUntilMs;
    uint8_t successBlinkToggleCount;

    PendingAction pendingAction;
    uint8_t pendingActionBlinkToggleCount;

    char setupApSsid[SETUP_SSID_BUFFER_SIZE];
    char serialBuffer[SERIAL_BUFFER_SIZE];
    size_t serialLength;
};

RuntimeState runtime = {};

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

const uint8_t* holeHelloZielMac() {
    return runtime.masterMacValid ? runtime.masterMac : BROADCAST_MAC;
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

void bestaetigeMitBeidenLeds(LedMode nextMode) {
    runtime.ledAckUntilMs = millis() + LED_ACK_DURATION_MS;
    runtime.ledModeAfterAck = nextMode;
    setzeLedPins(true, true);
}

bool hatAusstehendeAktion() {
    return runtime.pendingAction != PendingAction::None;
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

void berechneKalibrierstatus() {
    runtime.isCalibrated =
        isTravelTimeValid(runtime.travelTimeUpMs) && isTravelTimeValid(runtime.travelTimeDownMs);
    if (!runtime.isCalibrated) {
        runtime.coverPosition = 0;
    }
}

void loescheKalibrierungszustandImRuntime() {
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    runtime.isCalibrated = false;
    runtime.coverPosition = 0;
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

    const bool geaendert = !runtime.masterMacValid ||
                           memcmp(runtime.masterMac, senderMac, sizeof(runtime.masterMac)) != 0;
    setStoredMasterMac(senderMac);
    runtime.masterBound = true;

    if (!geaendert) {
        return true;
    }

    if (!speichereRuntimeStandMitRollback()) {
        logf("WARN", "Master-MAC aus HELLO_ACK konnte nicht persistiert werden");
        return false;
    }

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
    return sendePaket(holeHelloZielMac(), SH_MSG_HELLO, &payload, sizeof(payload), "HELLO");
}

bool sendeHeartbeat() {
    if (!runtime.masterBound || !darfFunkAktivSein()) return false;

    SmartHome::HeartbeatPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.uptime_s = millis() / 1000UL;

    if (!sendePaket(runtime.masterMac, SH_MSG_HEARTBEAT, &payload, sizeof(payload), "HEARTBEAT")) {
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
    payload.cover_position = (uint8_t)begrenzePosition(runtime.coverPosition);
    payload.cover_calibrated = runtime.isCalibrated ? 1U : 0U;
    payload.fault = 0U;
    payload.report_interval_s = (uint16_t)sanitizeStatusSendInterval(runtime.statusSendIntervalS);

    if (!sendePaket(runtime.masterMac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
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
    return sendePaket(runtime.masterMac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT");
}

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

    schreibePin(PIN_RELAY_A, false, RELAY_A_ACTIVE_HIGH);
    schreibePin(PIN_RELAY_B, false, RELAY_B_ACTIVE_HIGH);
    runtime.relayAActive = false;
    runtime.relayBActive = false;

    const int pin = pinFuerRichtung(direction);
    schreibePin(pin, true, activeHighFuerRichtung(direction));
    runtime.relayAActive = pin == PIN_RELAY_A;
    runtime.relayBActive = pin == PIN_RELAY_B;

    logf("INFO", "Relais fuer %s aktiv (%s)", toText(direction), grund ? grund : "ohne grund");
}

uint32_t fahrzeitFuerRichtung(CoverDirection direction) {
    if (direction == CoverDirection::Up) return runtime.travelTimeUpMs;
    if (direction == CoverDirection::Down) return runtime.travelTimeDownMs;
    return 0UL;
}

void aktualisierePositionsschaetzung(unsigned long jetztMs) {
    if (!runtime.isCalibrated) {
        runtime.coverPosition = 0;
        return;
    }

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

void stoppeFahrt(const char* grund) {
    aktualisierePositionsschaetzung(millis());
    setzeRelaisNeutral(grund);
    runtime.coverState = CoverState::Stopped;
    runtime.coverDirection = CoverDirection::None;
    runtime.movementStartedAtMs = 0UL;
    runtime.movementDeadlineAtMs = 0UL;
    runtime.movementStartPosition = runtime.coverPosition;
    runtime.movementTargetsEndPosition = false;
    runtime.movementTargetsIntermediatePosition = false;
    runtime.movementTargetPosition = runtime.coverPosition;
    setzeStateReportOffen();
}

void setzePositionAufEndlage(CoverDirection direction) {
    runtime.coverPosition = (!runtime.isCalibrated) ? 0 : (direction == CoverDirection::Up ? 100 : 0);
}

bool starteFahrt(CoverDirection direction, const char* grund, unsigned long autoStopMs, bool targetsEnd) {
    if (direction == CoverDirection::None || runtime.coverState == CoverState::Moving) return false;

    runtime.coverDirection = direction;
    runtime.coverState = CoverState::Moving;
    runtime.movementStartedAtMs = millis();
    runtime.movementDeadlineAtMs = autoStopMs > 0UL ? runtime.movementStartedAtMs + autoStopMs : 0UL;
    runtime.movementStartPosition = runtime.isCalibrated ? runtime.coverPosition : 0;
    runtime.movementTargetsEndPosition = targetsEnd;
    runtime.movementTargetsIntermediatePosition = false;
    runtime.movementTargetPosition = runtime.coverPosition;
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
    return true;
}

void stoppeFahrtMitEvent(const char* grund, uint8_t trigger) {
    if (runtime.coverState != CoverState::Moving) return;
    stoppeFahrt(grund);
    sendeCoverEvent(SH_EVENT_COVER_STOP, trigger, (uint8_t)begrenzePosition(runtime.coverPosition), 0U);
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

    stoppeFahrt(grund);

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

    stoppeFahrt("factory reset");
    runtime.calibrationMode = false;
    runtime.calibrationPhase = CalibrationPhase::Idle;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    nodeProvisioning.applyDefaultBasisValues();
    netZrlProvisioningHandler.loadDeviceDefaults();
    runtime.coverDirection = CoverDirection::None;
    runtime.coverPosition = 0;
    runtime.isCalibrated = false;
    runtime.masterBound = false;
    setzeStateReportOffen();

    if (!nodeProvisioning.clearStoredSettings()) {
        wendeSetupSnapshotAn(previousBasisSnapshot, previousDeviceSnapshot);
        logf("WARN", "Factory Reset konnte Persistenz nicht sauber loeschen, Vorzustand wiederhergestellt");
        return;
    }

    exitSetupMode("factory reset");
    setzeLedMode(LedMode::Off);
    setzeLedPins(false, false);
    logf("INFO", "Factory Reset ausgefuehrt");
}

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
        setzeLedMode(runtime.setupMode ? LedMode::Off : LedMode::Off);
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
        stoppeFahrt("kalibrierung down gestoppt");
        runtime.calibrationPhase = CalibrationPhase::WaitForUpStart;
        setzeLedMode(LedMode::UpBlink);
    } else {
        runtime.candidateTravelTimeUpMs = elapsedMs;
        stoppeFahrt("kalibrierung up gestoppt");
        runtime.calibrationPhase = CalibrationPhase::SuccessBlink;
        runtime.coverPosition = 100;
        runtime.successBlinkToggleCount = CALIBRATION_SUCCESS_BLINK_PULSES * 2U;
        runtime.ledBlinkState = false;
        runtime.ledLastTickMs = millis();
        setzeLedPins(false, false);
    }
}

void tickLeds() {
    const unsigned long jetztMs = millis();

    if (runtime.ledAckUntilMs > 0UL) {
        if (jetztMs < runtime.ledAckUntilMs) {
            setzeLedPins(true, true);
            return;
        }
        runtime.ledAckUntilMs = 0UL;
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
        sendeCoverEvent(SH_EVENT_COVER_UP, trigger, (uint8_t)begrenzePosition(runtime.coverPosition), 100U);
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
        sendeCoverEvent(SH_EVENT_COVER_DOWN, trigger, (uint8_t)begrenzePosition(runtime.coverPosition), 0U);
    }
    return gestartet;
}

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

    char buffer[560];
    snprintf(
        buffer,
        sizeof(buffer),
        "state=%s direction=%s relay_a=%s relay_b=%s calibration_mode=%s calibration_phase=%s setup_mode=%s setup_ap=%s setup_ssid=%s pending_action=%s pending_blinks=%u is_calibrated=%s master_mac=%s travel_time_up_ms=%lu travel_time_down_ms=%lu default_estimated_travel_time_ms=%lu relay_up_mapping=%s status_send_interval_s=%lu sensor_send_interval_s=%lu cover_position=%d",
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
        runtime.isCalibrated ? runtime.coverPosition : 0);
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
            if (!runtime.setupMode && !hatAusstehendeAktion()) setzeLedMode(LedMode::Off);
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

void verarbeiteBewegungsTimeouts() {
    if (runtime.coverState != CoverState::Moving) return;
    if (runtime.movementDeadlineAtMs == 0UL) return;
    if (millis() < runtime.movementDeadlineAtMs) return;

    if (runtime.calibrationMode && runtime.calibrationPhase == CalibrationPhase::MovingToTop) {
        stoppeFahrt("kalibrierung ausgangslage erreicht");
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
    stoppeFahrt("fahrzeit erreicht");
    if (!runtime.setupMode && !runtime.calibrationMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
    }
}

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
                if (!runtime.setupMode && !hatAusstehendeAktion()) setzeLedMode(LedMode::Off);
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
                    ackStatus = SH_ACK_OK;
                    break;

                case SH_COVER_CMD_SET_POSITION:
                    if (!runtime.isCalibrated) {
                        ackStatus = SH_ACK_REJECTED;
                    } else if (startePositionsfahrt((int16_t)payload.param2, "master set_position")) {
                        if (runtime.coverState == CoverState::Moving) {
                            sendeCoverEvent(
                                runtime.coverDirection == CoverDirection::Up ? SH_EVENT_COVER_UP : SH_EVENT_COVER_DOWN,
                                SH_TRIGGER_MASTER_CMD,
                                (uint8_t)begrenzePosition(runtime.coverPosition),
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

void onEspNowSend(const uint8_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

void initialisiereFunk() {
    if (runtime.funkBereit || runtime.setupMode) return;

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

void setup() {
    Serial.begin(115200);
    delay(150);

    runtime = {};
    runtime.coverState = CoverState::Stopped;
    runtime.coverDirection = CoverDirection::None;
    runtime.coverPosition = 0;
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

    initialisierePin(PIN_BUTTON_UP, INPUT);
    initialisierePin(PIN_BUTTON_DOWN, INPUT);
    initialisierePin(PIN_BUTTON_STOP, INPUT);

    const SmartHome::ShNodeProvisioning::NodeProvisioningConfig provisioningConfig = {
        SETUP_AP_PREFIX,
        STORAGE_NAMESPACE,
        STORAGE_KEY_NODE_BASIS,
        DEFAULT_STATUS_SEND_INTERVAL_S,
        DEFAULT_SENSOR_SEND_INTERVAL_S,
        MIN_SEND_INTERVAL_S,
        MAX_SEND_INTERVAL_S,
        1500UL,
        SETUP_AP_CHANNEL,
    };

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
