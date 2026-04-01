#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 1
#include "../../basetypes/bat_sen/main.cpp"

namespace {
bool kontakt_offen = false;
bool event_pending = false;
bool kontakt_init_ok = false;
int last_raw_level = LOW;
int stable_level = LOW;
unsigned long last_edge_ms = 0UL;

bool levelIstOffen(int level) {
#if BAT_SEN_WINDOW_CONTACT_OPEN_LEVEL_HIGH
    return level == HIGH;
#else
    return level == LOW;
#endif
}
}  // namespace

void device_init_io() {
#if BAT_SEN_WINDOW_CONTACT_USE_INPUT_PULLUP
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT_PULLUP);
#else
    pinMode(BAT_SEN_WINDOW_CONTACT_PIN, INPUT);
#endif

    stable_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);
    last_raw_level = stable_level;
    last_edge_ms = millis();
    kontakt_offen = levelIstOffen(stable_level);
    event_pending = false;
    kontakt_init_ok = true;

    logf(
        "INFO",
        "Window-Kontakt init: pin=%d status=%s",
        BAT_SEN_WINDOW_CONTACT_PIN,
        kontakt_offen ? "open" : "closed");
}

bool device_poll_inputs() {
    if (!kontakt_init_ok) return false;

    const unsigned long jetzt = millis();
    const int raw_level = digitalRead(BAT_SEN_WINDOW_CONTACT_PIN);

    if (raw_level != last_raw_level) {
        last_raw_level = raw_level;
        last_edge_ms = jetzt;
        return false;
    }

    if ((jetzt - last_edge_ms) < BAT_SEN_WINDOW_CONTACT_DEBOUNCE_MS) {
        return false;
    }

    if (raw_level == stable_level) {
        return false;
    }

    stable_level = raw_level;
    const bool neu_offen = levelIstOffen(stable_level);
    if (neu_offen == kontakt_offen) {
        return false;
    }

    kontakt_offen = neu_offen;
    event_pending = true;
    logf("INFO", "Fensterstatus geaendert: %s", kontakt_offen ? "open" : "closed");
    return true;
}

void device_build_state_channels(
    uint8_t* channelBool1,
    uint16_t* channelU16_1,
    uint8_t* channelMask1,
    bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = kontakt_offen ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = 0U;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !kontakt_init_ok;
}

bool device_map_event(
    uint8_t* eventType,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2)
{
    if (!event_pending) return false;
    event_pending = false;

    if (eventType != nullptr) {
        *eventType = kontakt_offen ? SH_EVENT_WINDOW_OPENED : SH_EVENT_WINDOW_CLOSED;
    }
    if (trigger != nullptr) *trigger = SH_TRIGGER_AUTO;
    if (param1 != nullptr) *param1 = kontakt_offen ? 1U : 0U;
    if (param2 != nullptr) *param2 = (uint16_t)((stable_level == HIGH) ? 1U : 0U);
    return true;
}

uint64_t device_wake_candidates() {
    if (BAT_SEN_WINDOW_CONTACT_PIN < 0 || BAT_SEN_WINDOW_CONTACT_PIN >= 64) {
        return 0ULL;
    }
    return (1ULL << (uint8_t)BAT_SEN_WINDOW_CONTACT_PIN);
}
