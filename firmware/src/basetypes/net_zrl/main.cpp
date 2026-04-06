#include <Arduino.h>
#include <Preferences.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../include/HardwarePinStandard.h"

namespace {

constexpr char DATEI_GERAET[] = "NET-ZRL";
constexpr char DATEI_VERSION[] = "0.2.0";

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
#define NET_ZRL_BUTTON_UP_PIN -1
#endif

#ifndef NET_ZRL_BUTTON_DOWN_PIN
#define NET_ZRL_BUTTON_DOWN_PIN -1
#endif

#ifndef NET_ZRL_BUTTON_STOP_PIN
#define NET_ZRL_BUTTON_STOP_PIN -1
#endif

#ifndef NET_ZRL_LED_UP_PIN
#define NET_ZRL_LED_UP_PIN -1
#endif

#ifndef NET_ZRL_LED_DOWN_PIN
#define NET_ZRL_LED_DOWN_PIN -1
#endif

#ifndef NET_ZRL_LED_ACTIVE_HIGH
#define NET_ZRL_LED_ACTIVE_HIGH 1
#endif

#ifndef NET_ZRL_BUTTON_ACTIVE_LOW
#define NET_ZRL_BUTTON_ACTIVE_LOW 1
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
constexpr unsigned long STOP_CALIBRATION_HOLD_MS = 5000UL;
constexpr unsigned long DOWN_SETUP_HOLD_MS = 15000UL;
constexpr unsigned long DOWN_RESET_HOLD_MS = 30000UL;
constexpr unsigned long DEFAULT_ESTIMATED_TRAVEL_TIME_MS = 100000UL;
constexpr unsigned long LED_BLINK_INTERVAL_MS = 300UL;
constexpr unsigned long LED_SUCCESS_INTERVAL_MS = 180UL;
constexpr unsigned long LED_ACK_DURATION_MS = 180UL;
constexpr uint32_t MIN_TRAVEL_TIME_MS = 1000UL;
constexpr uint32_t MAX_TRAVEL_TIME_MS = 180000UL;
constexpr uint8_t SUCCESS_BLINK_PULSES = 3U;
constexpr size_t SERIAL_BUFFER_SIZE = 128U;

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

struct RuntimeState {
    CoverState coverState;
    CoverDirection coverDirection;
    bool relayAActive;
    bool relayBActive;
    bool relayUpUsesRelayA;
    bool calibrationMode;
    bool setupMode;
    bool isCalibrated;
    int16_t coverPosition;
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    uint32_t candidateTravelTimeUpMs;
    uint32_t candidateTravelTimeDownMs;
    uint32_t defaultEstimatedTravelTimeMs;
    CalibrationPhase calibrationPhase;
    unsigned long movementStartedAtMs;
    unsigned long movementDeadlineAtMs;
    int16_t movementStartPosition;
    bool movementTargetsEndPosition;
    unsigned long lastButtonPollMs;
    bool lastUpButtonActive;
    bool lastDownButtonActive;
    bool lastStopButtonActive;
    unsigned long downPressedAtMs;
    unsigned long stopPressedAtMs;
    bool stopHoldConsumed;
    bool downSetupSignalActive;
    bool downResetTriggered;
    LedMode ledMode;
    LedMode ledModeAfterAck;
    bool ledBlinkState;
    unsigned long ledLastTickMs;
    unsigned long ledAckUntilMs;
    uint8_t successBlinkToggleCount;
    char serialBuffer[SERIAL_BUFFER_SIZE];
    size_t serialLength;
};

Preferences storage;
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

bool isTravelTimeValid(uint32_t valueMs) {
    return valueMs >= MIN_TRAVEL_TIME_MS && valueMs <= MAX_TRAVEL_TIME_MS;
}

uint32_t sanitizeEstimatedTravelTime(uint32_t valueMs) {
    return isTravelTimeValid(valueMs) ? valueMs : DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
}

bool leseButtonAktiv(int pin) {
    if (pin < 0) return false;
    const int raw = digitalRead(pin);
    return BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
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

void speicherePersistenz() {
    if (!storage.begin("net_zrl", false)) {
        logf("WARN", "Preferences konnte nicht geoeffnet werden");
        return;
    }
    if (runtime.travelTimeUpMs > 0UL) storage.putUInt("tt_up_ms", runtime.travelTimeUpMs);
    else storage.remove("tt_up_ms");
    if (runtime.travelTimeDownMs > 0UL) storage.putUInt("tt_dn_ms", runtime.travelTimeDownMs);
    else storage.remove("tt_dn_ms");
    storage.putUInt("est_ms", runtime.defaultEstimatedTravelTimeMs);
    storage.putBool("up_is_a", runtime.relayUpUsesRelayA);
    storage.end();
}

void ladePersistenz() {
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
    runtime.relayUpUsesRelayA = true;
    if (!storage.begin("net_zrl", true)) {
        logf("WARN", "Preferences konnte nicht gelesen werden");
        berechneKalibrierstatus();
        return;
    }
    runtime.travelTimeUpMs = storage.getUInt("tt_up_ms", 0UL);
    runtime.travelTimeDownMs = storage.getUInt("tt_dn_ms", 0UL);
    runtime.defaultEstimatedTravelTimeMs =
        sanitizeEstimatedTravelTime(storage.getUInt("est_ms", DEFAULT_ESTIMATED_TRAVEL_TIME_MS));
    runtime.relayUpUsesRelayA = storage.getBool("up_is_a", true);
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
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    runtime.isCalibrated = false;
    runtime.coverPosition = 0;
    berechneKalibrierstatus();
    speicherePersistenz();
}

void beendeKalibriermodus(const char* grund, bool messwerteUebernehmen) {
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
    speicherePersistenz();
    if (!runtime.setupMode) setzeLedMode(LedMode::Off);
}

void starteKalibriermodus() {
    if (runtime.coverState == CoverState::Moving || runtime.setupMode) return;
    runtime.calibrationMode = true;
    runtime.calibrationPhase = CalibrationPhase::MovingToTop;
    runtime.candidateTravelTimeUpMs = 0UL;
    runtime.candidateTravelTimeDownMs = 0UL;
    setzeLedMode(LedMode::BothBlink);
    if (!starteFahrt(
            CoverDirection::Up, "kalibrierung ausgangslage oben", runtime.defaultEstimatedTravelTimeMs, false)) {
        runtime.calibrationMode = false;
        runtime.calibrationPhase = CalibrationPhase::Idle;
        setzeLedMode(runtime.setupMode ? LedMode::BothBlink : LedMode::Off);
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
        runtime.successBlinkToggleCount = SUCCESS_BLINK_PULSES * 2U;
        runtime.ledBlinkState = false;
        runtime.ledLastTickMs = millis();
        setzeLedPins(false, false);
    }
}

void enterSetupMode() {
    if (runtime.setupMode || runtime.coverState == CoverState::Moving) return;
    runtime.setupMode = true;
    setzeLedMode(LedMode::BothBlink);
}

void exitSetupMode(const char* grund) {
    runtime.setupMode = false;
    if (!runtime.calibrationMode) setzeLedMode(LedMode::Off);
    logf("INFO", "Setup-Modus beendet (%s)", grund ? grund : "ohne grund");
}

void fuehreFactoryResetAus() {
    beendeKalibriermodus("factory reset", false);
    exitSetupMode("factory reset");
    runtime.relayUpUsesRelayA = true;
    runtime.defaultEstimatedTravelTimeMs = DEFAULT_ESTIMATED_TRAVEL_TIME_MS;
    setzeKalibrierungUngueltig();
    runtime.coverDirection = CoverDirection::None;
    runtime.downResetTriggered = true;
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
    char buffer[320];
    snprintf(
        buffer,
        sizeof(buffer),
        "state=%s direction=%s relay_a=%s relay_b=%s calibration_mode=%s calibration_phase=%s setup_mode=%s is_calibrated=%s travel_time_up_ms=%lu travel_time_down_ms=%lu default_estimated_travel_time_ms=%lu relay_up_mapping=%s cover_position=%d",
        toText(runtime.coverState),
        toText(runtime.coverDirection),
        runtime.relayAActive ? "true" : "false",
        runtime.relayBActive ? "true" : "false",
        runtime.calibrationMode ? "true" : "false",
        toText(runtime.calibrationPhase),
        runtime.setupMode ? "true" : "false",
        runtime.isCalibrated ? "true" : "false",
        (unsigned long)runtime.travelTimeUpMs,
        (unsigned long)runtime.travelTimeDownMs,
        (unsigned long)runtime.defaultEstimatedTravelTimeMs,
        runtime.relayUpUsesRelayA ? "relay_a" : "relay_b",
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
    Serial.println("  setup set tt_up_ms <ms>");
    Serial.println("  setup set tt_down_ms <ms>");
    Serial.println("  setup set default_ms <ms>");
    Serial.println("  setup set relay_up relay_a|relay_b");
    Serial.println("  setup reset_calibration");
    Serial.println("  help");
}

void verarbeiteSetupSet(const char* commandPrefix, const char* valueText) {
    if (!runtime.setupMode) {
        Serial.println("Setup-Modus ist nicht aktiv.");
        return;
    }
    uint32_t value = 0UL;
    if (strcmp(commandPrefix, "setup set relay_up") == 0) {
        if (strcmp(valueText, "relay_a") == 0) runtime.relayUpUsesRelayA = true;
        else if (strcmp(valueText, "relay_b") == 0) runtime.relayUpUsesRelayA = false;
        else {
            Serial.println("Ungueltige Relaiszuordnung.");
            return;
        }
        speicherePersistenz();
        Serial.println("Relaiszuordnung gespeichert.");
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
        runtime.defaultEstimatedTravelTimeMs = sanitizeEstimatedTravelTime(value);
    } else {
        Serial.println("Unbekannte Setup-Option.");
        return;
    }
    berechneKalibrierstatus();
    speicherePersistenz();
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
        if (runtime.calibrationMode &&
            (runtime.calibrationPhase == CalibrationPhase::MeasuringDown ||
             runtime.calibrationPhase == CalibrationPhase::MeasuringUp)) {
            uebernehmeKalibrierMessung(runtime.coverDirection);
        } else if (runtime.calibrationMode) {
            beendeKalibriermodus("serieller kalibrierabbruch", false);
        } else {
            stoppeFahrt("serieller stop");
            if (!runtime.setupMode) setzeLedMode(LedMode::Off);
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
        "setup set tt_up_ms ",
        "setup set tt_down_ms ",
        "setup set default_ms ",
        "setup set relay_up "
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
    if (!runtime.setupMode && !runtime.calibrationMode) setzeLedMode(LedMode::Off);
}

void behandleStopButton(bool stopActive, unsigned long jetztMs) {
    if (stopActive && !runtime.lastStopButtonActive) {
        runtime.stopPressedAtMs = jetztMs;
        runtime.stopHoldConsumed = false;
        if (runtime.coverState == CoverState::Moving) {
            if (runtime.calibrationMode &&
                (runtime.calibrationPhase == CalibrationPhase::MeasuringDown ||
                 runtime.calibrationPhase == CalibrationPhase::MeasuringUp)) {
                uebernehmeKalibrierMessung(runtime.coverDirection);
            } else {
                stoppeFahrt("lokaler taster stop");
                if (!runtime.setupMode) setzeLedMode(LedMode::Off);
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
        !runtime.calibrationMode && !runtime.setupMode &&
        runtime.stopPressedAtMs > 0UL &&
        (jetztMs - runtime.stopPressedAtMs) >= STOP_CALIBRATION_HOLD_MS) {
        runtime.stopHoldConsumed = true;
        starteKalibriermodus();
    }
    if (!stopActive && runtime.lastStopButtonActive) {
        runtime.stopPressedAtMs = 0UL;
        runtime.stopHoldConsumed = false;
    }
    runtime.lastStopButtonActive = stopActive;
}

void behandleUpButton(bool upActive) {
    if (!upActive || runtime.lastUpButtonActive) {
        runtime.lastUpButtonActive = upActive;
        return;
    }
    if (runtime.coverState == CoverState::Moving) {
        logf("INFO", "Up ignoriert: waehrend Fahrt ist nur stop relevant");
    } else if (runtime.setupMode) {
        logf("INFO", "Up ignoriert: Setup-Modus aktiv");
    } else if (runtime.calibrationMode) {
        if (runtime.calibrationPhase == CalibrationPhase::WaitForUpStart) {
            bestaetigeMitBeidenLeds(LedMode::UpOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringUp;
            starteFahrt(CoverDirection::Up, "kalibrierung messfahrt up", MAX_TRAVEL_TIME_MS, false);
        }
    } else {
        starteNormaleFahrtNachOben("lokaler taster up");
    }
    runtime.lastUpButtonActive = upActive;
}

void behandleDownButton(bool downActive, unsigned long jetztMs) {
    if (downActive && !runtime.lastDownButtonActive) {
        runtime.downPressedAtMs = jetztMs;
        runtime.downSetupSignalActive = false;
        runtime.downResetTriggered = false;
        if (runtime.coverState == CoverState::Moving) {
            logf("INFO", "Down ignoriert: waehrend Fahrt ist nur stop relevant");
        } else if (runtime.calibrationMode &&
                   runtime.calibrationPhase == CalibrationPhase::WaitForDownStart) {
            bestaetigeMitBeidenLeds(LedMode::DownOn);
            runtime.calibrationPhase = CalibrationPhase::MeasuringDown;
            starteFahrt(CoverDirection::Down, "kalibrierung messfahrt down", MAX_TRAVEL_TIME_MS, false);
        }
    }
    if (downActive && !runtime.calibrationMode && !runtime.setupMode &&
        runtime.coverState == CoverState::Stopped && runtime.downPressedAtMs > 0UL) {
        const unsigned long heldMs = jetztMs - runtime.downPressedAtMs;
        if (heldMs >= DOWN_SETUP_HOLD_MS && !runtime.downSetupSignalActive) {
            runtime.downSetupSignalActive = true;
            setzeLedMode(LedMode::BothBlink);
        }
        if (heldMs >= DOWN_RESET_HOLD_MS && !runtime.downResetTriggered) {
            fuehreFactoryResetAus();
        }
    }
    if (!downActive && runtime.lastDownButtonActive) {
        const unsigned long heldMs =
            runtime.downPressedAtMs > 0UL ? (jetztMs - runtime.downPressedAtMs) : 0UL;
        if (!runtime.calibrationMode && !runtime.setupMode &&
            runtime.coverState == CoverState::Stopped && !runtime.downResetTriggered) {
            if (heldMs >= DOWN_SETUP_HOLD_MS) {
                enterSetupMode();
            } else if (heldMs > 0UL) {
                starteNormaleFahrtNachUnten("lokaler taster down");
            }
        }
        runtime.downPressedAtMs = 0UL;
        runtime.downSetupSignalActive = false;
        if (!runtime.setupMode && !runtime.calibrationMode &&
            runtime.coverState == CoverState::Stopped) {
            setzeLedMode(LedMode::Off);
        }
    }
    runtime.lastDownButtonActive = downActive;
}

void pollButtons() {
    const unsigned long jetztMs = millis();
    if ((jetztMs - runtime.lastButtonPollMs) < BUTTON_POLL_MS) return;
    runtime.lastButtonPollMs = jetztMs;
    const bool upActive = leseButtonAktiv(PIN_BUTTON_UP);
    const bool downActive = leseButtonAktiv(PIN_BUTTON_DOWN);
    const bool stopActive = leseButtonAktiv(PIN_BUTTON_STOP);
    behandleStopButton(stopActive, jetztMs);
    behandleUpButton(upActive);
    behandleDownButton(downActive, jetztMs);
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
    initialisierePin(PIN_RELAY_A, OUTPUT);
    initialisierePin(PIN_RELAY_B, OUTPUT);
    initialisierePin(PIN_LED_UP, OUTPUT);
    initialisierePin(PIN_LED_DOWN, OUTPUT);
    setzeLedPins(false, false);
    setzeRelaisNeutral("boot");
    initialisierePin(PIN_BUTTON_UP, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    initialisierePin(PIN_BUTTON_DOWN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    initialisierePin(PIN_BUTTON_STOP, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    ladePersistenz();
    logf("INFO", "%s v%s startet", DATEI_GERAET, DATEI_VERSION);
    logf("INFO",
         "Pins relay_a=%d relay_b=%d button_up=%d button_down=%d button_stop=%d led_up=%d led_down=%d",
         PIN_RELAY_A,
         PIN_RELAY_B,
         PIN_BUTTON_UP,
         PIN_BUTTON_DOWN,
         PIN_BUTTON_STOP,
         PIN_LED_UP,
         PIN_LED_DOWN);
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
    tickLeds();
    delay(LOOP_DELAY_MS);
}
