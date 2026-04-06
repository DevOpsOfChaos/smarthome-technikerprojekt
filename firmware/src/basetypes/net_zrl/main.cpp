
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../include/HardwarePinStandard.h"

namespace {

constexpr char DATEI_GERAET[] = "NET-ZRL";
constexpr char DATEI_VERSION[] = "0.4.0";

#ifndef NET_ZRL_DEBUG_ENABLED
#define NET_ZRL_DEBUG_ENABLED 1
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
constexpr unsigned long SETUP_RESTART_DELAY_MS = 1500UL;
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
constexpr size_t MASTER_MAC_TEXT_LEN = 18U;
constexpr size_t SETUP_SSID_BUFFER_SIZE = 32U;
constexpr uint32_t SETUP_PERSIST_MAGIC = 0x5A524C31UL;
constexpr uint16_t SETUP_PERSIST_VERSION = 1U;
constexpr uint32_t SETUP_FLAG_MASTER_MAC = 0x00000001UL;
constexpr const char* STORAGE_NAMESPACE = "net_zrl";
constexpr const char* STORAGE_KEY_BLOB = "setup_v1";
constexpr const char* STORAGE_KEY_LEGACY_TT_UP = "tt_up_ms";
constexpr const char* STORAGE_KEY_LEGACY_TT_DOWN = "tt_dn_ms";
constexpr const char* STORAGE_KEY_LEGACY_EST = "est_ms";
constexpr const char* STORAGE_KEY_LEGACY_RELAY = "up_is_a";
constexpr const char* MASTER_MAC_ARG_PRIMARY = "master_mac";
constexpr const char* MASTER_MAC_ARG_ALIAS = "mac";
constexpr const char* SETUP_AP_PREFIX = "NET-ZRL-SETUP";
constexpr int SETUP_AP_CHANNEL = 1;

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

struct PersistedSetupData {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t flags;
    uint8_t masterMac[6];
    uint8_t reservedMac[2];
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
    uint8_t relayUpUsesRelayA;
    uint8_t reservedBytes[3];
};

struct SetupConfigSnapshot {
    bool masterMacValid;
    uint8_t masterMac[6];
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    bool relayUpUsesRelayA;
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
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
    int16_t coverPosition;
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
    int16_t movementStartPosition;
    bool movementTargetsEndPosition;

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

Preferences storage;
WebServer setupServer(80);
bool setupRoutesConfigured = false;
RuntimeState runtime = {};

void starteSetupAp();
void stoppeSetupAp(const char* grund);
void tickSetupServer();

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
    if (buffer == nullptr || bufferSize == 0U) return;
    if (!isValid || mac == nullptr || bufferSize < MASTER_MAC_TEXT_LEN) {
        buffer[0] = '\0';
        return;
    }

    snprintf(
        buffer,
        bufferSize,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

bool parseHexNibble(char ch, uint8_t& outValue) {
    if (ch >= '0' && ch <= '9') {
        outValue = (uint8_t)(ch - '0');
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        outValue = (uint8_t)(10 + (ch - 'A'));
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        outValue = (uint8_t)(10 + (ch - 'a'));
        return true;
    }
    return false;
}

bool parseMacText(const char* text, uint8_t outMac[6]) {
    if (text == nullptr || outMac == nullptr) return false;

    size_t length = strlen(text);
    while (length > 0U && (text[length - 1U] == ' ' || text[length - 1U] == '\t')) {
        --length;
    }

    size_t startIndex = 0U;
    while (startIndex < length && (text[startIndex] == ' ' || text[startIndex] == '\t')) {
        ++startIndex;
    }

    if ((length - startIndex) != 17U) {
        return false;
    }

    for (size_t index = 0U; index < 6U; ++index) {
        const size_t offset = startIndex + (index * 3U);
        uint8_t high = 0U;
        uint8_t low = 0U;
        if (!parseHexNibble(text[offset], high) || !parseHexNibble(text[offset + 1U], low)) {
            return false;
        }
        if (index < 5U && text[offset + 2U] != ':') {
            return false;
        }
        outMac[index] = (uint8_t)((high << 4U) | low);
    }
    return true;
}

String htmlEscape(const String& text) {
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

String buildStoredMasterMacText() {
    char masterMacText[MASTER_MAC_TEXT_LEN] = {0};
    formatMacText(runtime.masterMac, runtime.masterMacValid, masterMacText, sizeof(masterMacText));
    return String(masterMacText);
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

void holeSetupSnapshot(SetupConfigSnapshot& snapshot) {
    snapshot.masterMacValid = runtime.masterMacValid;
    memcpy(snapshot.masterMac, runtime.masterMac, sizeof(snapshot.masterMac));
    snapshot.travelTimeUpMs = runtime.travelTimeUpMs;
    snapshot.travelTimeDownMs = runtime.travelTimeDownMs;
    snapshot.defaultEstimatedTravelTimeMs = runtime.defaultEstimatedTravelTimeMs;
    snapshot.relayUpUsesRelayA = runtime.relayUpUsesRelayA;
    snapshot.statusSendIntervalS = runtime.statusSendIntervalS;
    snapshot.sensorSendIntervalS = runtime.sensorSendIntervalS;
}

void wendeSetupSnapshotAn(const SetupConfigSnapshot& snapshot) {
    runtime.masterMacValid = snapshot.masterMacValid;
    memcpy(runtime.masterMac, snapshot.masterMac, sizeof(runtime.masterMac));
    runtime.travelTimeUpMs = snapshot.travelTimeUpMs;
    runtime.travelTimeDownMs = snapshot.travelTimeDownMs;
    runtime.defaultEstimatedTravelTimeMs = snapshot.defaultEstimatedTravelTimeMs;
    runtime.relayUpUsesRelayA = snapshot.relayUpUsesRelayA;
    runtime.statusSendIntervalS = snapshot.statusSendIntervalS;
    runtime.sensorSendIntervalS = snapshot.sensorSendIntervalS;
    berechneKalibrierstatus();
}

PersistedSetupData bauePersistenzdatenAusRuntime() {
    PersistedSetupData data = {};
    data.magic = SETUP_PERSIST_MAGIC;
    data.version = SETUP_PERSIST_VERSION;
    data.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(runtime.defaultEstimatedTravelTimeMs);
    data.statusSendIntervalS = sanitizeStatusSendInterval(runtime.statusSendIntervalS);
    data.sensorSendIntervalS = sanitizeSensorSendInterval(runtime.sensorSendIntervalS);
    data.travelTimeUpMs = isTravelTimeValid(runtime.travelTimeUpMs) ? runtime.travelTimeUpMs : 0UL;
    data.travelTimeDownMs = isTravelTimeValid(runtime.travelTimeDownMs) ? runtime.travelTimeDownMs : 0UL;
    data.relayUpUsesRelayA = runtime.relayUpUsesRelayA ? 1U : 0U;
    if (runtime.masterMacValid) {
        data.flags |= SETUP_FLAG_MASTER_MAC;
        memcpy(data.masterMac, runtime.masterMac, sizeof(data.masterMac));
    }
    return data;
}

bool persistenzdatenGueltig(const PersistedSetupData& data) {
    return data.magic == SETUP_PERSIST_MAGIC && data.version == SETUP_PERSIST_VERSION;
}

void wendePersistenzdatenAn(const PersistedSetupData& data) {
    if ((data.flags & SETUP_FLAG_MASTER_MAC) != 0U) setStoredMasterMac(data.masterMac);
    else clearStoredMasterMac();
    runtime.travelTimeUpMs = isTravelTimeValid(data.travelTimeUpMs) ? data.travelTimeUpMs : 0UL;
    runtime.travelTimeDownMs = isTravelTimeValid(data.travelTimeDownMs) ? data.travelTimeDownMs : 0UL;
    runtime.defaultEstimatedTravelTimeMs =
        sanitizeEstimatedTravelTime(data.defaultEstimatedTravelTimeMs);
    runtime.relayUpUsesRelayA = data.relayUpUsesRelayA != 0U;
    runtime.statusSendIntervalS = sanitizeStatusSendInterval(data.statusSendIntervalS);
    runtime.sensorSendIntervalS = sanitizeSensorSendInterval(data.sensorSendIntervalS);
}

bool lesePersistenzBlob(PersistedSetupData& outData) {
    if (!storage.begin(STORAGE_NAMESPACE, true)) {
        logf("WARN", "Preferences konnte nicht fuer Blob-Lesen geoeffnet werden");
        return false;
    }

    bool success = false;
    if (storage.getBytesLength(STORAGE_KEY_BLOB) == sizeof(PersistedSetupData)) {
        PersistedSetupData candidate = {};
        if (storage.getBytes(STORAGE_KEY_BLOB, &candidate, sizeof(candidate)) == sizeof(candidate) &&
            persistenzdatenGueltig(candidate)) {
            outData = candidate;
            success = true;
        }
    }

    storage.end();
    return success;
}

bool loeschePersistenz() {
    if (!storage.begin(STORAGE_NAMESPACE, false)) {
        logf("WARN", "Preferences konnte nicht fuer Persistenz-Loeschen geoeffnet werden");
        return false;
    }

    bool success = true;
    success = storage.remove(STORAGE_KEY_BLOB) && success;
    success = storage.remove(STORAGE_KEY_LEGACY_TT_UP) && success;
    success = storage.remove(STORAGE_KEY_LEGACY_TT_DOWN) && success;
    success = storage.remove(STORAGE_KEY_LEGACY_EST) && success;
    success = storage.remove(STORAGE_KEY_LEGACY_RELAY) && success;
    storage.end();
    return success;
}

bool speicherePersistenz() {
    PersistedSetupData previousData = {};
    const bool hadPreviousBlob = lesePersistenzBlob(previousData);
    const PersistedSetupData nextData = bauePersistenzdatenAusRuntime();

    if (!storage.begin(STORAGE_NAMESPACE, false)) {
        logf("WARN", "Preferences konnte nicht geoeffnet werden");
        return false;
    }

    const bool writeOk =
        storage.putBytes(STORAGE_KEY_BLOB, &nextData, sizeof(nextData)) == sizeof(nextData);
    if (writeOk) {
        storage.remove(STORAGE_KEY_LEGACY_TT_UP);
        storage.remove(STORAGE_KEY_LEGACY_TT_DOWN);
        storage.remove(STORAGE_KEY_LEGACY_EST);
        storage.remove(STORAGE_KEY_LEGACY_RELAY);
    }
    storage.end();

    if (writeOk) return true;

    logf("WARN", "Persistenz-Blob konnte nicht geschrieben werden. Vorzustand wird wiederhergestellt.");
    if (!storage.begin(STORAGE_NAMESPACE, false)) {
        logf("WARN", "Preferences fuer Rollback konnten nicht geoeffnet werden");
        return false;
    }

    if (hadPreviousBlob) storage.putBytes(STORAGE_KEY_BLOB, &previousData, sizeof(previousData));
    else storage.remove(STORAGE_KEY_BLOB);
    storage.end();
    return false;
}

void ladePersistenz() {
    clearStoredMasterMac();
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
    runtime.relayUpUsesRelayA = true;
    runtime.statusSendIntervalS = DEFAULT_STATUS_SEND_INTERVAL_S;
    runtime.sensorSendIntervalS = DEFAULT_SENSOR_SEND_INTERVAL_S;

    PersistedSetupData persistedData = {};
    if (lesePersistenzBlob(persistedData)) {
        wendePersistenzdatenAn(persistedData);
        berechneKalibrierstatus();
        return;
    }

    if (!storage.begin(STORAGE_NAMESPACE, true)) {
        logf("WARN", "Preferences konnte nicht gelesen werden");
        berechneKalibrierstatus();
        return;
    }

    runtime.travelTimeUpMs = storage.getUInt(STORAGE_KEY_LEGACY_TT_UP, 0UL);
    runtime.travelTimeDownMs = storage.getUInt(STORAGE_KEY_LEGACY_TT_DOWN, 0UL);
    runtime.defaultEstimatedTravelTimeMs =
        sanitizeEstimatedTravelTime(storage.getUInt(STORAGE_KEY_LEGACY_EST, DEFAULT_ESTIMATED_TRAVEL_TIME_MS));
    runtime.relayUpUsesRelayA = storage.getBool(STORAGE_KEY_LEGACY_RELAY, true);
    storage.end();

    if (!isTravelTimeValid(runtime.travelTimeUpMs)) runtime.travelTimeUpMs = 0UL;
    if (!isTravelTimeValid(runtime.travelTimeDownMs)) runtime.travelTimeDownMs = 0UL;

    berechneKalibrierstatus();
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
    runtime.movementStartedAtMs = 0UL;
    runtime.movementDeadlineAtMs = 0UL;
    runtime.movementStartPosition = runtime.coverPosition;
    runtime.movementTargetsEndPosition = false;
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
    setzeRelaisFuerRichtung(direction, grund);
    return true;
}

void setzeKalibrierungUngueltig() {
    SetupConfigSnapshot previousSnapshot = {};
    holeSetupSnapshot(previousSnapshot);

    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    runtime.isCalibrated = false;
    runtime.coverPosition = 0;
    berechneKalibrierstatus();

    if (!speicherePersistenz()) {
        wendeSetupSnapshotAn(previousSnapshot);
        logf("WARN", "Kalibrierung konnte nicht geloescht werden, Vorzustand wiederhergestellt");
    }
}

void enterSetupMode() {
    if (runtime.setupMode || runtime.coverState == CoverState::Moving) return;
    runtime.setupMode = true;
    setzeLedMode(LedMode::Off);
    setzeLedPins(false, false);
    starteSetupAp();
    logf("INFO", "Setup-Modus aktiviert");
}

void exitSetupMode(const char* grund) {
    runtime.setupMode = false;
    runtime.restartPending = false;
    stoppeSetupAp(grund);
    if (!runtime.calibrationMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
        setzeLedPins(false, false);
    }
    logf("INFO", "Setup-Modus beendet (%s)", grund ? grund : "ohne grund");
}

void beendeKalibriermodus(const char* grund, bool messwerteUebernehmen) {
    SetupConfigSnapshot previousSnapshot = {};
    holeSetupSnapshot(previousSnapshot);

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
    if (!speicherePersistenz()) {
        wendeSetupSnapshotAn(previousSnapshot);
        runtime.calibrationMode = false;
        runtime.calibrationPhase = CalibrationPhase::Idle;
        logf("WARN", "Kalibrierwerte konnten nicht persistiert werden, Vorzustand wiederhergestellt");
    }

    if (!runtime.setupMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
    }
}

void fuehreFactoryResetAus() {
    stoppeFahrt("factory reset");
    runtime.calibrationMode = false;
    runtime.calibrationPhase = CalibrationPhase::Idle;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    clearStoredMasterMac();
    runtime.relayUpUsesRelayA = true;
    runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
    runtime.statusSendIntervalS = DEFAULT_STATUS_SEND_INTERVAL_S;
    runtime.sensorSendIntervalS = DEFAULT_SENSOR_SEND_INTERVAL_S;
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.coverDirection = CoverDirection::None;
    runtime.coverPosition = 0;
    runtime.isCalibrated = false;
    runtime.restartPending = false;
    loeschePersistenz();
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

void starteNormaleFahrtNachOben(const char* grund) {
    unsigned long autoStopMs = 0UL;
    if (runtime.isCalibrated && isTravelTimeValid(runtime.travelTimeUpMs)) {
        autoStopMs = (runtime.travelTimeUpMs * 12UL) / 10UL;
    }

    if (starteFahrt(CoverDirection::Up, grund, autoStopMs, autoStopMs > 0UL) &&
        !runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(LedMode::UpOn);
    }
}

void starteNormaleFahrtNachUnten(const char* grund) {
    unsigned long autoStopMs = 0UL;
    if (runtime.isCalibrated && isTravelTimeValid(runtime.travelTimeDownMs)) {
        autoStopMs = (runtime.travelTimeDownMs * 12UL) / 10UL;
    }

    if (starteFahrt(CoverDirection::Down, grund, autoStopMs, autoStopMs > 0UL) &&
        !runtime.setupMode && !runtime.calibrationMode) {
        setzeLedMode(LedMode::DownOn);
    }
}

bool parseUIntValue(const char* text, uint32_t& outValue) {
    if (text == nullptr || *text == '\0') return false;

    char* endPtr = nullptr;
    const unsigned long parsed = strtoul(text, &endPtr, 10);
    if (endPtr == text || *endPtr != '\0') return false;

    outValue = (uint32_t)parsed;
    return true;
}

String baueSetupInfoText(const String& infoText) {
    const String escapedInfo = htmlEscape(infoText);
    const String escapedSsid = htmlEscape(String(runtime.setupApSsid));
    const String escapedIp = htmlEscape(WiFi.softAPIP().toString());

    String html;
    html.reserve(256U);
    html += F("<div class=\"info\">");
    html += escapedInfo;
    if (runtime.setupApActive) {
        html += F("<br>AP: <strong>");
        html += escapedSsid;
        html += F("</strong><br>URL: <strong>http://");
        html += escapedIp;
        html += F("/</strong>");
    }
    html += F("</div>");
    return html;
}

String buildSetupPage(
    const String& masterMacText,
    const String& travelTimeUpText,
    const String& travelTimeDownText,
    const String& defaultTravelTimeText,
    const String& relayMappingText,
    const String& statusIntervalText,
    const String& sensorIntervalText,
    const String& infoText,
    const String& errorText) {
    String page;
    page.reserve(7200U);

    const String escapedMasterMac = htmlEscape(masterMacText);
    const String escapedTravelTimeUp = htmlEscape(travelTimeUpText);
    const String escapedTravelTimeDown = htmlEscape(travelTimeDownText);
    const String escapedDefaultTravelTime = htmlEscape(defaultTravelTimeText);
    const String escapedStatusInterval = htmlEscape(statusIntervalText);
    const String escapedSensorInterval = htmlEscape(sensorIntervalText);
    const String infoBlock = baueSetupInfoText(
        infoText.length() > 0U ? infoText : String(F("Setup-Modus aktiv.")));
    const String escapedError = htmlEscape(errorText);
    const bool relayASelected = relayMappingText != "relay_b";

    page += F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
    page += F("<title>NET-ZRL Setup</title>");
    page += F("<style>");
    page += F(":root{--bg:#f4efe6;--card:#fffdf8;--text:#1f1e1a;--muted:#6f6a60;--line:#d8d0c3;--accent:#1e6f5c;--accent2:#155243;--error:#b3261e;--ok:#1e6f5c;--input:#fff;}");
    page += F("*{box-sizing:border-box}html,body{margin:0;padding:0;background:linear-gradient(180deg,#efe6d8 0%,#f7f2e9 100%);color:var(--text);font-family:Arial,Helvetica,sans-serif;min-height:100%}");
    page += F("body{padding:18px 14px}.wrap{max-width:460px;margin:0 auto}.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:20px 16px;box-shadow:0 18px 40px rgba(0,0,0,.08)}");
    page += F("h1{margin:0 0 8px;font-size:1.35rem;line-height:1.2}p{margin:0 0 14px;color:var(--muted);line-height:1.45}.grid{display:grid;gap:14px}.field{display:grid;gap:7px}");
    page += F("label{font-weight:700;font-size:.92rem}.hint{font-size:.8rem;color:var(--muted);line-height:1.35}.info,.error{border-radius:14px;padding:12px 13px;font-size:.87rem;line-height:1.4}");
    page += F(".info{background:#edf8f4;border:1px solid #c7e6dc;color:var(--ok)}.error{background:#fff1f0;border:1px solid #f0c1bc;color:var(--error);margin-bottom:14px}");
    page += F("input,select{width:100%;min-height:48px;border-radius:14px;border:1px solid var(--line);background:var(--input);padding:0 13px;font-size:1rem;color:var(--text)}");
    page += F("button{width:100%;min-height:52px;border:0;border-radius:14px;background:linear-gradient(180deg,var(--accent),var(--accent2));color:#fff;font-size:1rem;font-weight:700;margin-top:6px}");
    page += F(".footer{margin-top:14px;font-size:.78rem;color:var(--muted)}");
    page += F("</style></head><body><div class=\"wrap\"><form class=\"card\" method=\"post\" action=\"/save\" id=\"setupForm\" novalidate>");
    page += F("<h1>NET-ZRL Provisioning</h1>");
    page += F("<p>Schlichte lokale Inbetriebnahme fuer Master-Bindung, Fahrzeiten, Relais-Mapping und Sendeintervalle.</p>");
    if (errorText.length() > 0U) {
        page += F("<div class=\"error\">");
        page += escapedError;
        page += F("</div>");
    }
    page += infoBlock;
    page += F("<div class=\"grid\">");
    page += F("<div class=\"field\"><label for=\"master_mac\">Master-MAC</label>");
    page += F("<input id=\"master_mac\" name=\"master_mac\" type=\"text\" maxlength=\"17\" autocapitalize=\"characters\" autocomplete=\"off\" spellcheck=\"false\" placeholder=\"AA:BB:CC:DD:EE:FF\" value=\"");
    page += escapedMasterMac;
    page += F("\"><div class=\"hint\">Wird per Link auch ueber <code>?master_mac=...</code> oder <code>?mac=...</code> uebernommen.</div></div>");
    page += F("<div class=\"field\"><label for=\"travel_time_up_ms\">travel_time_up_ms</label>");
    page += F("<input id=\"travel_time_up_ms\" name=\"travel_time_up_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" placeholder=\"leer = unkalibriert\" value=\"");
    page += escapedTravelTimeUp;
    page += F("\"><div class=\"hint\">Leer lassen, wenn noch keine valide Aufwaerts-Kalibrierung vorliegt.</div></div>");
    page += F("<div class=\"field\"><label for=\"travel_time_down_ms\">travel_time_down_ms</label>");
    page += F("<input id=\"travel_time_down_ms\" name=\"travel_time_down_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" placeholder=\"leer = unkalibriert\" value=\"");
    page += escapedTravelTimeDown;
    page += F("\"><div class=\"hint\">Leer lassen, wenn noch keine valide Abwaerts-Kalibrierung vorliegt.</div></div>");
    page += F("<div class=\"field\"><label for=\"default_estimated_travel_time_ms\">default_estimated_travel_time_ms</label>");
    page += F("<input id=\"default_estimated_travel_time_ms\" name=\"default_estimated_travel_time_ms\" type=\"number\" min=\"1000\" max=\"180000\" step=\"1\" inputmode=\"numeric\" value=\"");
    page += escapedDefaultTravelTime;
    page += F("\"><div class=\"hint\">Fallback fuer die automatische Anfangsfahrt, Standard: 100000 ms.</div></div>");
    page += F("<div class=\"field\"><label for=\"relay_up_mapping\">Relais-Zuordnung fuer Richtung up</label>");
    page += F("<select id=\"relay_up_mapping\" name=\"relay_up_mapping\">");
    page += relayASelected ? F("<option value=\"relay_a\" selected>relay_a = up</option>") : F("<option value=\"relay_a\">relay_a = up</option>");
    page += relayASelected ? F("<option value=\"relay_b\">relay_b = up</option>") : F("<option value=\"relay_b\" selected>relay_b = up</option>");
    page += F("</select><div class=\"hint\">Das andere Relais wird automatisch als down verwendet.</div></div>");
    page += F("<div class=\"field\"><label for=\"status_send_interval_s\">Status-Sendeintervall (s)</label>");
    page += F("<input id=\"status_send_interval_s\" name=\"status_send_interval_s\" type=\"number\" min=\"10\" max=\"65535\" step=\"1\" inputmode=\"numeric\" value=\"");
    page += escapedStatusInterval;
    page += F("\"><div class=\"hint\">Persistiert jetzt separat, auch wenn der echte Laufzeitversand fuer net_zrl noch nicht existiert.</div></div>");
    page += F("<div class=\"field\"><label for=\"sensor_send_interval_s\">Sensor-Sendeintervall (s)</label>");
    page += F("<input id=\"sensor_send_interval_s\" name=\"sensor_send_interval_s\" type=\"number\" min=\"10\" max=\"65535\" step=\"1\" inputmode=\"numeric\" value=\"");
    page += escapedSensorInterval;
    page += F("\"><div class=\"hint\">Persistiert separat fuer den spaeteren Sensor-/Status-Pfad.</div></div>");
    page += F("</div><button id=\"saveBtn\" type=\"submit\">Speichern und neu starten</button>");
    page += F("<div class=\"footer\">Nach erfolgreichem Speichern wird der Setup-AP sauber beendet und das Geraet neu gestartet.</div>");
    page += F("</form></div>");
    page += F("<script>(function(){const form=document.getElementById('setupForm');const mac=document.getElementById('master_mac');");
    page += F("function norm(v){return v.trim().toUpperCase().replace(/-/g,':');}");
    page += F("function valid(v){return /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(v);}mac.addEventListener('blur',function(){mac.value=norm(mac.value);});");
    page += F("form.addEventListener('submit',function(e){mac.value=norm(mac.value);if(!valid(mac.value)){e.preventDefault();alert('Master-MAC ist ungueltig.');}});})();</script>");
    page += F("</body></html>");

    return page;
}

void sendeSetupForm(
    const String& masterMacText,
    const String& travelTimeUpText,
    const String& travelTimeDownText,
    const String& defaultTravelTimeText,
    const String& relayMappingText,
    const String& statusIntervalText,
    const String& sensorIntervalText,
    const String& infoText,
    const String& errorText,
    int statusCode = 200) {
    setupServer.send(
        statusCode,
        "text/html; charset=utf-8",
        buildSetupPage(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            infoText,
            errorText));
}

bool leseAngeforderteMasterMac(String& outValue, const char*& outSourceArg) {
    if (setupServer.hasArg(MASTER_MAC_ARG_PRIMARY)) {
        outValue = setupServer.arg(MASTER_MAC_ARG_PRIMARY);
        outSourceArg = MASTER_MAC_ARG_PRIMARY;
        return true;
    }
    if (setupServer.hasArg(MASTER_MAC_ARG_ALIAS)) {
        outValue = setupServer.arg(MASTER_MAC_ARG_ALIAS);
        outSourceArg = MASTER_MAC_ARG_ALIAS;
        return true;
    }
    outValue = String();
    outSourceArg = nullptr;
    return false;
}

bool parsePflichtZahlFeld(
    const String& rawText,
    uint32_t minValue,
    uint32_t maxValue,
    uint32_t& outValue) {
    if (rawText.length() == 0U) return false;
    return parseUIntValue(rawText.c_str(), outValue) && outValue >= minValue && outValue <= maxValue;
}

bool parseOptionalTravelTimeField(const String& rawText, uint32_t& outValue, bool& isSet) {
    if (rawText.length() == 0U) {
        isSet = false;
        outValue = 0UL;
        return true;
    }
    isSet = parseUIntValue(rawText.c_str(), outValue) && isTravelTimeValid(outValue);
    return isSet;
}

void handleSetupRoot() {
    String masterMacText = buildStoredMasterMacText();
    String travelTimeUpText = travelTimeText(runtime.travelTimeUpMs);
    String travelTimeDownText = travelTimeText(runtime.travelTimeDownMs);
    String defaultTravelTimeText = String(runtime.defaultEstimatedTravelTimeMs);
    String relayMappingText = runtime.relayUpUsesRelayA ? "relay_a" : "relay_b";
    String statusIntervalText = String(runtime.statusSendIntervalS);
    String sensorIntervalText = String(runtime.sensorSendIntervalS);
    String infoText =
        runtime.restartPending ? String(F("Setup-Daten gespeichert. Neustart laeuft gleich an."))
                               : String(F("Setup-Modus aktiv."));
    String errorText;

    String requestedMasterMac;
    const char* requestedArg = nullptr;
    if (leseAngeforderteMasterMac(requestedMasterMac, requestedArg)) {
        uint8_t parsedMac[6] = {0};
        if (parseMacText(requestedMasterMac.c_str(), parsedMac)) {
            char normalizedMacText[MASTER_MAC_TEXT_LEN] = {0};
            formatMacText(parsedMac, true, normalizedMacText, sizeof(normalizedMacText));
            masterMacText = String(normalizedMacText);
            infoText = F("Master-MAC aus Query uebernommen.");
            logf("INFO", "Master-MAC aus Query gelesen (%s): %s", requestedArg, masterMacText.c_str());
        } else {
            errorText = F("Die uebergebene Master-MAC ist ungueltig.");
            logf("WARN",
                 "Ungueltige Master-MAC aus Query verworfen (%s): %s",
                 requestedArg,
                 requestedMasterMac.c_str());
        }
    }

    sendeSetupForm(
        masterMacText,
        travelTimeUpText,
        travelTimeDownText,
        defaultTravelTimeText,
        relayMappingText,
        statusIntervalText,
        sensorIntervalText,
        infoText,
        errorText);
}

void handleSetupSave() {
    const String masterMacText = setupServer.arg(MASTER_MAC_ARG_PRIMARY);
    const String travelTimeUpText = setupServer.arg("travel_time_up_ms");
    const String travelTimeDownText = setupServer.arg("travel_time_down_ms");
    const String defaultTravelTimeText = setupServer.arg("default_estimated_travel_time_ms");
    const String relayMappingText = setupServer.arg("relay_up_mapping");
    const String statusIntervalText = setupServer.arg("status_send_interval_s");
    const String sensorIntervalText = setupServer.arg("sensor_send_interval_s");

    uint8_t parsedMasterMac[6] = {0};
    if (!parseMacText(masterMacText.c_str(), parsedMasterMac)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("Master-MAC ist ungueltig. Erwartet wird AA:BB:CC:DD:EE:FF."),
            400);
        return;
    }

    uint32_t travelTimeUpMs = 0UL;
    uint32_t travelTimeDownMs = 0UL;
    bool travelTimeUpSet = false;
    bool travelTimeDownSet = false;
    if (!parseOptionalTravelTimeField(travelTimeUpText, travelTimeUpMs, travelTimeUpSet)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("travel_time_up_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms oder leer."),
            400);
        return;
    }
    if (!parseOptionalTravelTimeField(travelTimeDownText, travelTimeDownMs, travelTimeDownSet)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("travel_time_down_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms oder leer."),
            400);
        return;
    }

    uint32_t defaultTravelTimeMs = 0UL;
    if (!parsePflichtZahlFeld(
            defaultTravelTimeText, MIN_TRAVEL_TIME_MS, MAX_TRAVEL_TIME_MS, defaultTravelTimeMs)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("default_estimated_travel_time_ms ist ungueltig. Erlaubt sind 1000 bis 180000 ms."),
            400);
        return;
    }

    if (relayMappingText != "relay_a" && relayMappingText != "relay_b") {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("Relais-Zuordnung ist ungueltig."),
            400);
        return;
    }

    uint32_t statusIntervalS = 0UL;
    if (!parsePflichtZahlFeld(statusIntervalText, MIN_SEND_INTERVAL_S, MAX_SEND_INTERVAL_S, statusIntervalS)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("Status-Sendeintervall ist ungueltig. Erlaubt sind 10 bis 65535 Sekunden."),
            400);
        return;
    }

    uint32_t sensorIntervalS = 0UL;
    if (!parsePflichtZahlFeld(sensorIntervalText, MIN_SEND_INTERVAL_S, MAX_SEND_INTERVAL_S, sensorIntervalS)) {
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("Sensor-Sendeintervall ist ungueltig. Erlaubt sind 10 bis 65535 Sekunden."),
            400);
        return;
    }

    SetupConfigSnapshot previousSnapshot = {};
    holeSetupSnapshot(previousSnapshot);

    setStoredMasterMac(parsedMasterMac);
    runtime.travelTimeUpMs = travelTimeUpSet ? travelTimeUpMs : 0UL;
    runtime.travelTimeDownMs = travelTimeDownSet ? travelTimeDownMs : 0UL;
    runtime.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(defaultTravelTimeMs);
    runtime.relayUpUsesRelayA = relayMappingText == "relay_a";
    runtime.statusSendIntervalS = sanitizeStatusSendInterval(statusIntervalS);
    runtime.sensorSendIntervalS = sanitizeSensorSendInterval(sensorIntervalS);
    berechneKalibrierstatus();

    if (!speicherePersistenz()) {
        wendeSetupSnapshotAn(previousSnapshot);
        sendeSetupForm(
            masterMacText,
            travelTimeUpText,
            travelTimeDownText,
            defaultTravelTimeText,
            relayMappingText,
            statusIntervalText,
            sensorIntervalText,
            String(),
            F("Speichern in NVS fehlgeschlagen. Vorzustand wurde wiederhergestellt."),
            500);
        return;
    }

    char savedMasterMacText[MASTER_MAC_TEXT_LEN] = {0};
    formatMacText(runtime.masterMac, runtime.masterMacValid, savedMasterMacText, sizeof(savedMasterMacText));
    logf("INFO", "Master-MAC gespeichert: %s", savedMasterMacText);
    logf("INFO",
         "Setup-Werte gespeichert: tt_up_ms=%lu tt_down_ms=%lu default_ms=%lu relay_up=%s status_s=%lu sensor_s=%lu",
         (unsigned long)runtime.travelTimeUpMs,
         (unsigned long)runtime.travelTimeDownMs,
         (unsigned long)runtime.defaultEstimatedTravelTimeMs,
         runtime.relayUpUsesRelayA ? "relay_a" : "relay_b",
         (unsigned long)runtime.statusSendIntervalS,
         (unsigned long)runtime.sensorSendIntervalS);

    runtime.restartPending = true;
    runtime.restartRequestedAtMs = millis();
    sendeSetupForm(
        String(savedMasterMacText),
        travelTimeText(runtime.travelTimeUpMs),
        travelTimeText(runtime.travelTimeDownMs),
        String(runtime.defaultEstimatedTravelTimeMs),
        runtime.relayUpUsesRelayA ? "relay_a" : "relay_b",
        String(runtime.statusSendIntervalS),
        String(runtime.sensorSendIntervalS),
        F("Setup-Daten gespeichert. Das Geraet startet gleich neu."),
        String());
}

void konfiguriereSetupRouten() {
    if (setupRoutesConfigured) return;
    setupServer.on("/", HTTP_GET, handleSetupRoot);
    setupServer.on("/save", HTTP_POST, handleSetupSave);
    setupServer.onNotFound(handleSetupRoot);
    setupRoutesConfigured = true;
}

void stoppeSetupAp(const char* grund) {
    if (!runtime.setupApActive) return;

    setupServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_MODE_NULL);
    runtime.setupApActive = false;
    runtime.setupApSsid[0] = '\0';
    logf("INFO", "Setup-AP beendet (%s)", grund ? grund : "ohne grund");
}

void starteSetupAp() {
    konfiguriereSetupRouten();
    runtime.restartPending = false;

    const uint64_t chipId = ESP.getEfuseMac();
    snprintf(
        runtime.setupApSsid,
        sizeof(runtime.setupApSsid),
        "%s-%02X%02X%02X",
        SETUP_AP_PREFIX,
        (unsigned)((chipId >> 16U) & 0xFFU),
        (unsigned)((chipId >> 8U) & 0xFFU),
        (unsigned)(chipId & 0xFFU));

    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_MODE_NULL);
    delay(25);
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP(runtime.setupApSsid, nullptr, SETUP_AP_CHANNEL)) {
        runtime.setupApActive = false;
        logf("WARN", "Setup-AP konnte nicht gestartet werden");
        return;
    }

    setupServer.begin();
    runtime.setupApActive = true;
    logf("INFO",
         "Setup-AP aktiv: ssid=%s ip=%s url=http://%s/",
         runtime.setupApSsid,
         WiFi.softAPIP().toString().c_str(),
         WiFi.softAPIP().toString().c_str());
}

void tickSetupServer() {
    if (runtime.setupApActive) {
        setupServer.handleClient();
    }

    if (runtime.restartPending &&
        (millis() - runtime.restartRequestedAtMs) >= SETUP_RESTART_DELAY_MS) {
        runtime.restartPending = false;
        logf("INFO", "Setup-Neustart wird ausgefuehrt");
        stoppeSetupAp("save restart");
        delay(50);
        ESP.restart();
    }
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

    SetupConfigSnapshot previousSnapshot = {};
    holeSetupSnapshot(previousSnapshot);

    uint32_t value = 0UL;
    if (strcmp(commandPrefix, "setup set relay_up") == 0) {
        if (strcmp(valueText, "relay_a") == 0) runtime.relayUpUsesRelayA = true;
        else if (strcmp(valueText, "relay_b") == 0) runtime.relayUpUsesRelayA = false;
        else {
            Serial.println("Ungueltige Relaiszuordnung.");
            return;
        }

        if (!speicherePersistenz()) {
            wendeSetupSnapshotAn(previousSnapshot);
            Serial.println("Speichern fehlgeschlagen, Vorzustand wiederhergestellt.");
            return;
        }
        Serial.println("Relaiszuordnung gespeichert.");
        return;
    }

    if (strcmp(commandPrefix, "setup set master_mac") == 0) {
        uint8_t parsedMac[6] = {0};
        if (!parseMacText(valueText, parsedMac)) {
            Serial.println("Ungueltige Master-MAC.");
            return;
        }
        setStoredMasterMac(parsedMac);
        if (!speicherePersistenz()) {
            wendeSetupSnapshotAn(previousSnapshot);
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

    if (strcmp(commandPrefix, "setup set tt_up_ms") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("tt_up_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.travelTimeUpMs = value;
    } else if (strcmp(commandPrefix, "setup set tt_down_ms") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("tt_down_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.travelTimeDownMs = value;
    } else if (strcmp(commandPrefix, "setup set default_ms") == 0) {
        if (!isTravelTimeValid(value)) {
            Serial.println("default_ms ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(value);
    } else if (strcmp(commandPrefix, "setup set status_interval_s") == 0) {
        if (!isSendIntervalValid(value)) {
            Serial.println("status_interval_s ausserhalb des gueltigen Bereichs.");
            return;
        }
        runtime.statusSendIntervalS = value;
    } else if (strcmp(commandPrefix, "setup set sensor_interval_s") == 0) {
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
    if (!speicherePersistenz()) {
        wendeSetupSnapshotAn(previousSnapshot);
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
            stoppeFahrt("serieller stop");
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
        starteNormaleFahrtNachOben("serieller up");
        return;
    }
    if (strcmp(line, "down") == 0) {
        if (runtime.calibrationMode && runtime.calibrationPhase == CalibrationPhase::WaitForDownStart) {
            bestaetigeMitBeidenLeds(LedMode::DownOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringDown;
            starteFahrt(CoverDirection::Down, "kalibrierung messfahrt down", MAX_TRAVEL_TIME_MS, false);
            return;
        }
        starteNormaleFahrtNachUnten("serieller down");
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
                stoppeFahrt("lokaler taster stop");
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

    initialisierePin(PIN_RELAY_A, OUTPUT);
    initialisierePin(PIN_RELAY_B, OUTPUT);
    initialisierePin(PIN_LED_UP, OUTPUT);
    initialisierePin(PIN_LED_DOWN, OUTPUT);

    setzeLedPins(false, false);
    setzeRelaisNeutral("boot");

    initialisierePin(PIN_BUTTON_UP, INPUT);
    initialisierePin(PIN_BUTTON_DOWN, INPUT);
    initialisierePin(PIN_BUTTON_STOP, INPUT);

    ladePersistenz();

    logf("INFO", "%s v%s startet", DATEI_GERAET, DATEI_VERSION);
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

    tickSetupServer();
    tickLeds();
    delay(LOOP_DELAY_MS);
}
