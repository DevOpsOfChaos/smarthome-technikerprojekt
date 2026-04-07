
#include <Arduino.h>
#include <ShNodeProvisioning.h>
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
constexpr const char* STORAGE_KEY_LEGACY_BLOB = "setup_v1";
constexpr const char* STORAGE_KEY_LEGACY_TT_UP = "tt_up_ms";
constexpr const char* STORAGE_KEY_LEGACY_TT_DOWN = "tt_dn_ms";
constexpr const char* STORAGE_KEY_LEGACY_EST = "est_ms";
constexpr const char* STORAGE_KEY_LEGACY_RELAY = "up_is_a";
constexpr const char* SETUP_AP_PREFIX = "NET-ZRL-SETUP";
constexpr int SETUP_AP_CHANNEL = 1;
constexpr uint32_t NET_ZRL_SETUP_MAGIC = 0x5A524C32UL;
constexpr uint16_t NET_ZRL_SETUP_VERSION = 1U;
constexpr uint32_t LEGACY_SETUP_PERSIST_MAGIC = 0x5A524C31UL;
constexpr uint16_t LEGACY_SETUP_PERSIST_VERSION = 1U;
constexpr uint32_t LEGACY_SETUP_FLAG_MASTER_MAC = 0x00000001UL;

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

struct LegacyPersistedSetupData {
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

bool legacyPersistenzdatenGueltig(const LegacyPersistedSetupData& data) {
    return data.magic == LEGACY_SETUP_PERSIST_MAGIC &&
           data.version == LEGACY_SETUP_PERSIST_VERSION;
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

        LegacyPersistedSetupData legacy = {};
        if (prefs.getBytesLength(STORAGE_KEY_LEGACY_BLOB) == sizeof(LegacyPersistedSetupData) &&
            prefs.getBytes(STORAGE_KEY_LEGACY_BLOB, &legacy, sizeof(legacy)) == sizeof(legacy) &&
            legacyPersistenzdatenGueltig(legacy)) {
            runtime.travelTimeUpMs = isTravelTimeValid(legacy.travelTimeUpMs) ? legacy.travelTimeUpMs : 0UL;
            runtime.travelTimeDownMs = isTravelTimeValid(legacy.travelTimeDownMs) ? legacy.travelTimeDownMs : 0UL;
            runtime.defaultEstimatedTravelTimeMs =
                sanitizeEstimatedTravelTime(legacy.defaultEstimatedTravelTimeMs);
            runtime.relayUpUsesRelayA = legacy.relayUpUsesRelayA != 0U;
            berechneKalibrierstatus();
            return true;
        }

        runtime.travelTimeUpMs = prefs.getUInt(STORAGE_KEY_LEGACY_TT_UP, 0UL);
        runtime.travelTimeDownMs = prefs.getUInt(STORAGE_KEY_LEGACY_TT_DOWN, 0UL);
        runtime.defaultEstimatedTravelTimeMs =
            sanitizeEstimatedTravelTime(
                prefs.getUInt(STORAGE_KEY_LEGACY_EST, DEFAULT_ESTIMATED_TRAVEL_TIME_MS));
        runtime.relayUpUsesRelayA = prefs.getBool(STORAGE_KEY_LEGACY_RELAY, true);

        if (!isTravelTimeValid(runtime.travelTimeUpMs)) runtime.travelTimeUpMs = 0UL;
        if (!isTravelTimeValid(runtime.travelTimeDownMs)) runtime.travelTimeDownMs = 0UL;
        berechneKalibrierstatus();

        return runtime.travelTimeUpMs > 0UL || runtime.travelTimeDownMs > 0UL ||
               runtime.defaultEstimatedTravelTimeMs != DEFAULT_ESTIMATED_TRAVEL_TIME_MS ||
               !runtime.relayUpUsesRelayA;
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

        const bool writeOk =
            prefs.putBytes(STORAGE_KEY_NET_ZRL_BLOB, &data, sizeof(data)) == sizeof(data);
        if (!writeOk) {
            return false;
        }

        prefs.remove(STORAGE_KEY_LEGACY_BLOB);
        prefs.remove(STORAGE_KEY_LEGACY_TT_UP);
        prefs.remove(STORAGE_KEY_LEGACY_TT_DOWN);
        prefs.remove(STORAGE_KEY_LEGACY_EST);
        prefs.remove(STORAGE_KEY_LEGACY_RELAY);
        return true;
    }

    bool clearDeviceSettings(Preferences& prefs) override {
        prefs.remove(STORAGE_KEY_NET_ZRL_BLOB);
        prefs.remove(STORAGE_KEY_LEGACY_BLOB);
        prefs.remove(STORAGE_KEY_LEGACY_TT_UP);
        prefs.remove(STORAGE_KEY_LEGACY_TT_DOWN);
        prefs.remove(STORAGE_KEY_LEGACY_EST);
        prefs.remove(STORAGE_KEY_LEGACY_RELAY);
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

    bool loadLegacyBasisSettings(
        Preferences& prefs,
        SmartHome::ShNodeProvisioning::NodeBasisSnapshot& outBasis) override {
        LegacyPersistedSetupData legacy = {};
        if (prefs.getBytesLength(STORAGE_KEY_LEGACY_BLOB) != sizeof(LegacyPersistedSetupData)) {
            return false;
        }

        if (prefs.getBytes(STORAGE_KEY_LEGACY_BLOB, &legacy, sizeof(legacy)) != sizeof(legacy) ||
            !legacyPersistenzdatenGueltig(legacy)) {
            return false;
        }

        outBasis = {};
        outBasis.masterMacValid = (legacy.flags & LEGACY_SETUP_FLAG_MASTER_MAC) != 0U;
        memcpy(outBasis.masterMac, legacy.masterMac, sizeof(outBasis.masterMac));
        outBasis.statusSendIntervalS = sanitizeStatusSendInterval(legacy.statusSendIntervalS);
        outBasis.sensorSendIntervalS = sanitizeSensorSendInterval(legacy.sensorSendIntervalS);
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
    SmartHome::ShNodeProvisioning::NodeBasisSnapshot previousBasisSnapshot = {};
    NetZrlSetupSnapshot previousDeviceSnapshot = {};
    holeSetupSnapshot(previousBasisSnapshot, previousDeviceSnapshot);

    loescheKalibrierungszustandImRuntime();

    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        logf("WARN", "Kalibrierung konnte nicht geloescht werden, Vorzustand wiederhergestellt");
    }
}

void enterSetupMode() {
    if (runtime.setupMode || runtime.coverState == CoverState::Moving) return;
    setzeLedMode(LedMode::Off);
    setzeLedPins(false, false);
    nodeProvisioning.enterSetupMode();
    if (runtime.setupMode && runtime.setupApActive) {
        logf("INFO", "Setup-Modus aktiviert");
    } else {
        logf("WARN", "Setup-Modus konnte nicht sauber aktiviert werden");
    }
}

void exitSetupMode(const char* grund) {
    nodeProvisioning.exitSetupMode(grund);
    if (!runtime.calibrationMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
        setzeLedPins(false, false);
    }
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
    if (!speicherePersistenzMitRollback(previousBasisSnapshot, previousDeviceSnapshot)) {
        runtime.calibrationMode = false;
        runtime.calibrationPhase = CalibrationPhase::Idle;
        logf("WARN", "Kalibrierwerte konnten nicht persistiert werden, Vorzustand wiederhergestellt");
    }

    if (!runtime.setupMode && !hatAusstehendeAktion()) {
        setzeLedMode(LedMode::Off);
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

    nodeProvisioning.update();
    tickLeds();
    delay(LOOP_DELAY_MS);
}
