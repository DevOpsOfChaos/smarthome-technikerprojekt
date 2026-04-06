#include <Arduino.h>
#include <Preferences.h>
#include <stdarg.h>
#include <string.h>

#include "../../../include/HardwarePinStandard.h"

namespace {

constexpr char DATEI_GERAET[] = "NET-ZRL";
constexpr char DATEI_VERSION[] = "0.1.0";

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

#ifndef NET_ZRL_BUTTON_ACTIVE_LOW
#define NET_ZRL_BUTTON_ACTIVE_LOW 1
#endif

constexpr bool DEBUG_AKTIV = NET_ZRL_DEBUG_ENABLED != 0;
constexpr int PIN_RELAY_UP = NET_ZRL_RELAY_UP_PIN;
constexpr int PIN_RELAY_DOWN = NET_ZRL_RELAY_DOWN_PIN;
constexpr int PIN_BUTTON_UP = NET_ZRL_BUTTON_UP_PIN;
constexpr int PIN_BUTTON_DOWN = NET_ZRL_BUTTON_DOWN_PIN;
constexpr int PIN_BUTTON_STOP = NET_ZRL_BUTTON_STOP_PIN;
constexpr bool BUTTON_ACTIVE_LOW = NET_ZRL_BUTTON_ACTIVE_LOW != 0;

constexpr unsigned long LOOP_DELAY_MS = 10UL;
constexpr unsigned long BUTTON_POLL_MS = 20UL;
constexpr unsigned long REVERSE_NEUTRAL_MS = 150UL;
constexpr uint32_t MIN_TRAVEL_TIME_MS = 1000UL;
constexpr uint32_t MAX_TRAVEL_TIME_MS = 180000UL;
constexpr size_t SERIAL_BUFFER_SIZE = 96U;

enum class CoverState : uint8_t {
    Stopped = 0,
    Moving = 1
};

enum class CoverDirection : uint8_t {
    None = 0,
    Up = 1,
    Down = 2
};

struct RuntimeState {
    CoverState coverState;
    CoverDirection coverDirection;
    bool relayUpActive;
    bool relayDownActive;
    bool calibrationMode;
    bool isCalibrated;
    bool coverPositionValid;
    int16_t coverPosition;
    uint32_t travelTimeUpMs;
    uint32_t travelTimeDownMs;
    unsigned long movementStartedAtMs;
    int16_t movementStartPosition;
    unsigned long lastButtonPollMs;
    bool lastUpButtonActive;
    bool lastDownButtonActive;
    bool lastStopButtonActive;
    char serialBuffer[SERIAL_BUFFER_SIZE];
    size_t serialLength;
};

Preferences storage;
RuntimeState runtime = {};

void logf(const char* level, const char* format, ...) {
    if (!DEBUG_AKTIV) return;

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

const char* toText(CoverState state) {
    switch (state) {
        case CoverState::Moving: return "moving";
        case CoverState::Stopped:
        default: return "stopped";
    }
}

const char* toText(CoverDirection direction) {
    switch (direction) {
        case CoverDirection::Up: return "up";
        case CoverDirection::Down: return "down";
        case CoverDirection::None:
        default: return "null";
    }
}

const char* schreibePositionText(char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0U) return "";
    if (!runtime.coverPositionValid) {
        snprintf(buffer, bufferSize, "null");
    } else {
        snprintf(buffer, bufferSize, "%d", runtime.coverPosition);
    }
    return buffer;
}

bool isTravelTimeValid(uint32_t valueMs) {
    return valueMs >= MIN_TRAVEL_TIME_MS && valueMs <= MAX_TRAVEL_TIME_MS;
}

bool leseButtonAktiv(int pin) {
    if (pin < 0) return false;
    const int raw = digitalRead(pin);
    return BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
}

void schreibeRelayPin(int pin, bool active, bool activeHigh) {
    if (pin < 0) return;
    digitalWrite(pin, active == activeHigh ? HIGH : LOW);
}

void berechneKalibrierstatus() {
    runtime.isCalibrated =
        isTravelTimeValid(runtime.travelTimeUpMs) &&
        isTravelTimeValid(runtime.travelTimeDownMs);
}

void speichereKalibrierung() {
    if (!storage.begin("net_zrl", false)) {
        logf("WARN", "Preferences konnte nicht geoeffnet werden");
        return;
    }

    if (runtime.travelTimeUpMs > 0UL) {
        storage.putUInt("tt_up_ms", runtime.travelTimeUpMs);
    } else {
        storage.remove("tt_up_ms");
    }

    if (runtime.travelTimeDownMs > 0UL) {
        storage.putUInt("tt_dn_ms", runtime.travelTimeDownMs);
    } else {
        storage.remove("tt_dn_ms");
    }

    storage.putBool("cal_ok", runtime.isCalibrated);
    storage.end();
}

void ladeKalibrierung() {
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.isCalibrated = false;

    if (!storage.begin("net_zrl", true)) {
        logf("WARN", "Preferences konnte nicht gelesen werden");
        return;
    }

    runtime.travelTimeUpMs = storage.getUInt("tt_up_ms", 0UL);
    runtime.travelTimeDownMs = storage.getUInt("tt_dn_ms", 0UL);
    storage.end();

    if (!isTravelTimeValid(runtime.travelTimeUpMs)) {
        runtime.travelTimeUpMs = 0UL;
    }

    if (!isTravelTimeValid(runtime.travelTimeDownMs)) {
        runtime.travelTimeDownMs = 0UL;
    }

    berechneKalibrierstatus();
}

void setzeRelaisNeutral(const char* grund) {
    schreibeRelayPin(PIN_RELAY_UP, false, NET_ZRL_RELAY_UP_ACTIVE_HIGH != 0);
    schreibeRelayPin(PIN_RELAY_DOWN, false, NET_ZRL_RELAY_DOWN_ACTIVE_HIGH != 0);
    runtime.relayUpActive = false;
    runtime.relayDownActive = false;
    logf("INFO", "Relais neutral (%s)", grund ? grund : "ohne grund");
}

void setzeRelaisSicher(bool upActive, bool downActive, const char* grund) {
    if (upActive && downActive) {
        logf("WARN", "gleichzeitige Relaisanforderung blockiert");
        setzeRelaisNeutral("harte sperre");
        return;
    }

    schreibeRelayPin(PIN_RELAY_UP, false, NET_ZRL_RELAY_UP_ACTIVE_HIGH != 0);
    schreibeRelayPin(PIN_RELAY_DOWN, false, NET_ZRL_RELAY_DOWN_ACTIVE_HIGH != 0);

    runtime.relayUpActive = false;
    runtime.relayDownActive = false;

    if (upActive) {
        schreibeRelayPin(PIN_RELAY_UP, true, NET_ZRL_RELAY_UP_ACTIVE_HIGH != 0);
        runtime.relayUpActive = true;
    } else if (downActive) {
        schreibeRelayPin(PIN_RELAY_DOWN, true, NET_ZRL_RELAY_DOWN_ACTIVE_HIGH != 0);
        runtime.relayDownActive = true;
    }

    logf("INFO",
         "Relais up=%s down=%s (%s)",
         runtime.relayUpActive ? "an" : "aus",
         runtime.relayDownActive ? "an" : "aus",
         grund ? grund : "ohne grund");
}

void uebernehmePositionsschaetzung(unsigned long jetztMs) {
    if (!runtime.isCalibrated) return;
    if (!runtime.coverPositionValid) return;
    if (runtime.coverState != CoverState::Moving) return;
    if (runtime.coverDirection == CoverDirection::None) return;

    const unsigned long elapsedMs = jetztMs - runtime.movementStartedAtMs;
    const uint32_t travelTimeMs =
        runtime.coverDirection == CoverDirection::Up ? runtime.travelTimeUpMs : runtime.travelTimeDownMs;

    if (!isTravelTimeValid(travelTimeMs) || runtime.movementStartPosition < 0) {
        runtime.coverPositionValid = false;
        runtime.coverPosition = -1;
        return;
    }

    const long delta = (long)((elapsedMs * 100UL) / travelTimeMs);
    long nextPosition = runtime.movementStartPosition;

    if (runtime.coverDirection == CoverDirection::Up) {
        nextPosition += delta;
    } else {
        nextPosition -= delta;
    }

    if (nextPosition < 0L) nextPosition = 0L;
    if (nextPosition > 100L) nextPosition = 100L;
    runtime.coverPosition = (int16_t)nextPosition;
}

void stoppeFahrt(const char* grund) {
    const unsigned long jetztMs = millis();
    char positionText[16];
    uebernehmePositionsschaetzung(jetztMs);
    setzeRelaisNeutral(grund);
    runtime.coverState = CoverState::Stopped;
    runtime.movementStartedAtMs = 0UL;
    runtime.movementStartPosition = runtime.coverPositionValid ? runtime.coverPosition : -1;
    logf("INFO",
         "stop state=%s direction=%s position=%s",
         toText(runtime.coverState),
         toText(runtime.coverDirection),
         schreibePositionText(positionText, sizeof(positionText)));
}

void starteFahrt(CoverDirection direction, const char* grund) {
    if (direction == CoverDirection::None) {
        stoppeFahrt("ungueltige richtung");
        return;
    }

    if (runtime.coverState == CoverState::Moving && runtime.coverDirection == direction) {
        logf("INFO", "Fahrt %s laeuft bereits", toText(direction));
        return;
    }

    const bool richtungswechsel =
        runtime.coverState == CoverState::Moving &&
        runtime.coverDirection != CoverDirection::None &&
        runtime.coverDirection != direction;

    if (runtime.coverState == CoverState::Moving) {
        stoppeFahrt("vor richtungswechsel");
        if (richtungswechsel) {
            delay(REVERSE_NEUTRAL_MS);
        }
    }

    runtime.coverDirection = direction;
    runtime.coverState = CoverState::Moving;
    runtime.movementStartedAtMs = millis();
    runtime.movementStartPosition = runtime.coverPositionValid ? runtime.coverPosition : -1;

    if (direction == CoverDirection::Up) {
        setzeRelaisSicher(true, false, grund);
    } else {
        setzeRelaisSicher(false, true, grund);
    }

    logf("INFO", "start %s (%s)", toText(direction), grund ? grund : "ohne grund");
}

void setzeKalibrierungUngueltig() {
    runtime.travelTimeUpMs = 0UL;
    runtime.travelTimeDownMs = 0UL;
    runtime.isCalibrated = false;
    runtime.coverPositionValid = false;
    runtime.coverPosition = -1;
    runtime.movementStartPosition = -1;
    speichereKalibrierung();
}

void starteKalibriermodus() {
    stoppeFahrt("kalibrierstart");
    runtime.calibrationMode = true;
    setzeKalibrierungUngueltig();
    logf("INFO", "Kalibriermodus aktiv");
}

void beendeKalibriermodus(const char* grund) {
    stoppeFahrt(grund);
    runtime.calibrationMode = false;
    berechneKalibrierstatus();
    speichereKalibrierung();
    logf("INFO", "Kalibriermodus beendet (%s)", grund ? grund : "ohne grund");
}

void uebernehmeKalibrierMessung(CoverDirection direction) {
    if (!runtime.calibrationMode) {
        logf("WARN", "Kalibrierwert verworfen: Kalibriermodus nicht aktiv");
        return;
    }

    if (runtime.coverState != CoverState::Moving || runtime.coverDirection != direction) {
        logf("WARN", "Kalibrierwert verworfen: passende Fahrt laeuft nicht");
        return;
    }

    const unsigned long jetztMs = millis();
    const uint32_t elapsedMs = (uint32_t)(jetztMs - runtime.movementStartedAtMs);
    if (!isTravelTimeValid(elapsedMs)) {
        logf("WARN", "Kalibrierwert verworfen: %lu ms ausserhalb des gueltigen Bereichs", elapsedMs);
        stoppeFahrt("ungueltige kalibrierzeit");
        return;
    }

    if (direction == CoverDirection::Up) {
        runtime.travelTimeUpMs = elapsedMs;
        runtime.coverPosition = 100;
    } else {
        runtime.travelTimeDownMs = elapsedMs;
        runtime.coverPosition = 0;
    }

    runtime.coverPositionValid = true;
    stoppeFahrt("kalibrierbestaetigung");
    berechneKalibrierstatus();
    speichereKalibrierung();

    logf("INFO",
         "Kalibrierwert %s gespeichert: %lu ms",
         toText(direction),
         (unsigned long)elapsedMs);

    if (runtime.isCalibrated) {
        runtime.calibrationMode = false;
        logf("INFO", "Kalibrierung vollstaendig");
    }
}

void gibStatusAus() {
    char buffer[256];
    char positionText[16];
    snprintf(
        buffer,
        sizeof(buffer),
        "state=%s direction=%s relay_up=%s relay_down=%s calibration_mode=%s is_calibrated=%s travel_time_up_ms=%lu travel_time_down_ms=%lu cover_position=%s",
        toText(runtime.coverState),
        toText(runtime.coverDirection),
        runtime.relayUpActive ? "true" : "false",
        runtime.relayDownActive ? "true" : "false",
        runtime.calibrationMode ? "true" : "false",
        runtime.isCalibrated ? "true" : "false",
        (unsigned long)runtime.travelTimeUpMs,
        (unsigned long)runtime.travelTimeDownMs,
        schreibePositionText(positionText, sizeof(positionText)));
    Serial.println(buffer);
}

void gibHilfeAus() {
    Serial.println("Befehle:");
    Serial.println("  status");
    Serial.println("  up");
    Serial.println("  down");
    Serial.println("  stop");
    Serial.println("  cal start");
    Serial.println("  cal save up");
    Serial.println("  cal save down");
    Serial.println("  cal abort");
    Serial.println("  help");
}

void verarbeiteBefehl(char* line) {
    if (line == nullptr) return;

    while (*line == ' ' || *line == '\t') {
        ++line;
    }

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
        stoppeFahrt("serieller stop");
        return;
    }

    if (strcmp(line, "up") == 0) {
        starteFahrt(CoverDirection::Up, "serieller up");
        return;
    }

    if (strcmp(line, "down") == 0) {
        starteFahrt(CoverDirection::Down, "serieller down");
        return;
    }

    if (strcmp(line, "cal start") == 0) {
        starteKalibriermodus();
        return;
    }

    if (strcmp(line, "cal abort") == 0) {
        beendeKalibriermodus("kalibrierabbruch");
        return;
    }

    if (strcmp(line, "cal save up") == 0) {
        uebernehmeKalibrierMessung(CoverDirection::Up);
        return;
    }

    if (strcmp(line, "cal save down") == 0) {
        uebernehmeKalibrierMessung(CoverDirection::Down);
        return;
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

void pruefeButtonFlanke(bool aktuellAktiv, bool& letzterZustand, void (*aktion)()) {
    if (aktuellAktiv && !letzterZustand && aktion != nullptr) {
        aktion();
    }
    letzterZustand = aktuellAktiv;
}

void buttonUpAktion() {
    starteFahrt(CoverDirection::Up, "lokaler taster up");
}

void buttonDownAktion() {
    starteFahrt(CoverDirection::Down, "lokaler taster down");
}

void buttonStopAktion() {
    stoppeFahrt("lokaler taster stop");
}

void pollButtons() {
    const unsigned long jetztMs = millis();
    if ((jetztMs - runtime.lastButtonPollMs) < BUTTON_POLL_MS) {
        return;
    }
    runtime.lastButtonPollMs = jetztMs;

    pruefeButtonFlanke(leseButtonAktiv(PIN_BUTTON_UP), runtime.lastUpButtonActive, buttonUpAktion);
    pruefeButtonFlanke(leseButtonAktiv(PIN_BUTTON_DOWN), runtime.lastDownButtonActive, buttonDownAktion);
    pruefeButtonFlanke(leseButtonAktiv(PIN_BUTTON_STOP), runtime.lastStopButtonActive, buttonStopAktion);
}

void initialisierePin(int pin, uint8_t mode) {
    if (pin >= 0) {
        pinMode(pin, mode);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(150);

    runtime = {};
    runtime.coverState = CoverState::Stopped;
    runtime.coverDirection = CoverDirection::None;
    runtime.coverPositionValid = false;
    runtime.coverPosition = -1;
    runtime.movementStartPosition = -1;

    initialisierePin(PIN_RELAY_UP, OUTPUT);
    initialisierePin(PIN_RELAY_DOWN, OUTPUT);
    setzeRelaisNeutral("boot");

    initialisierePin(PIN_BUTTON_UP, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    initialisierePin(PIN_BUTTON_DOWN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
    initialisierePin(PIN_BUTTON_STOP, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT);

    ladeKalibrierung();

    logf("INFO", "%s v%s startet", DATEI_GERAET, DATEI_VERSION);
    logf("INFO",
         "Pins relay_up=%d relay_down=%d button_up=%d button_down=%d button_stop=%d",
         PIN_RELAY_UP,
         PIN_RELAY_DOWN,
         PIN_BUTTON_UP,
         PIN_BUTTON_DOWN,
         PIN_BUTTON_STOP);
    logf("INFO",
         "Persistenz travel_time_up_ms=%lu travel_time_down_ms=%lu is_calibrated=%s",
         (unsigned long)runtime.travelTimeUpMs,
         (unsigned long)runtime.travelTimeDownMs,
         runtime.isCalibrated ? "true" : "false");

    gibHilfeAus();
    gibStatusAus();
}

void loop() {
    verarbeiteSerielleBefehle();
    pollButtons();

    if (runtime.coverState == CoverState::Moving && runtime.isCalibrated && runtime.coverPositionValid) {
        uebernehmePositionsschaetzung(millis());
    }

    delay(LOOP_DELAY_MS);
}
