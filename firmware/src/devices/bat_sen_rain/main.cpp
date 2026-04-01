#include <Arduino.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 1
#include "../../basetypes/bat_sen/BatSenRuntime.h"

static_assert(BAT_SEN_RAIN_SIGNAL_PIN >= 0, "bat_sen_rain braucht einen gueltigen ADC-Pin.");
static_assert(BAT_SEN_RAIN_STATE_DELTA_RAW > 0U, "BAT_SEN_RAIN_STATE_DELTA_RAW muss groesser als 0 sein.");
#if BAT_SEN_RAIN_LEVEL_HIGH_IS_WET
static_assert(
    BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW <= BAT_SEN_RAIN_WET_THRESHOLD_RAW,
    "Bei HIGH=wet muss CLEAR <= WET sein.");
#else
static_assert(
    BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW >= BAT_SEN_RAIN_WET_THRESHOLD_RAW,
    "Bei LOW=wet muss CLEAR >= WET sein.");
#endif

namespace {
bool regen_erkannt = false;
bool event_pending = false;
bool rain_init_ok = false;
uint16_t rain_raw = 0U;
bool event_regenstatus = false;
uint16_t event_raw = 0U;
unsigned long letzte_probe_ms = 0UL;

uint16_t absDiffU16(uint16_t a, uint16_t b) {
    return (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

uint16_t leseRainRaw() {
    int raw = analogRead(BAT_SEN_RAIN_SIGNAL_PIN);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    return (uint16_t)raw;
}

bool istRegenZustand(uint16_t raw, bool bisherRegen) {
#if BAT_SEN_RAIN_LEVEL_HIGH_IS_WET
    if (bisherRegen) {
        return raw >= BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW;
    }
    return raw >= BAT_SEN_RAIN_WET_THRESHOLD_RAW;
#else
    if (bisherRegen) {
        return raw <= BAT_SEN_RAIN_CLEAR_THRESHOLD_RAW;
    }
    return raw <= BAT_SEN_RAIN_WET_THRESHOLD_RAW;
#endif
}
}  // namespace

void device_init_io() {
    pinMode(BAT_SEN_RAIN_SIGNAL_PIN, INPUT);
    adcAttachPin(BAT_SEN_RAIN_SIGNAL_PIN);
    analogSetPinAttenuation(BAT_SEN_RAIN_SIGNAL_PIN, ADC_11db);

    rain_raw = leseRainRaw();
    regen_erkannt = istRegenZustand(rain_raw, false);
    event_pending = false;
    event_regenstatus = regen_erkannt;
    event_raw = rain_raw;
    letzte_probe_ms = millis();
    rain_init_ok = true;

    logf(
        "INFO",
        "Rain init: pin=%d raw=%u status=%s",
        BAT_SEN_RAIN_SIGNAL_PIN,
        rain_raw,
        regen_erkannt ? "wet" : "dry");
}

bool device_poll_inputs() {
    if (!rain_init_ok) return false;

    const unsigned long jetzt = millis();
    if ((jetzt - letzte_probe_ms) < BAT_SEN_RAIN_SAMPLE_INTERVAL_MS) {
        return false;
    }
    letzte_probe_ms = jetzt;

    const uint16_t neuerRaw = leseRainRaw();
    const bool neuerRegenstatus = istRegenZustand(neuerRaw, regen_erkannt);
    const bool statusGeaendert = neuerRegenstatus != regen_erkannt;
    const bool rawGeaendert = absDiffU16(neuerRaw, rain_raw) >= BAT_SEN_RAIN_STATE_DELTA_RAW;

    if (!statusGeaendert && !rawGeaendert) {
        return false;
    }

    rain_raw = neuerRaw;

    if (statusGeaendert) {
        regen_erkannt = neuerRegenstatus;
        event_pending = true;
        event_regenstatus = neuerRegenstatus;
        event_raw = neuerRaw;
        logf("INFO", "Rain status geaendert: %s (raw=%u)", regen_erkannt ? "wet" : "dry", rain_raw);
    }

    return true;
}

void device_build_state_channels(
    uint8_t* channelBool1,
    uint16_t* channelU16_1,
    uint8_t* channelMask1,
    bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = regen_erkannt ? 1U : 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = rain_raw;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = !rain_init_ok;
}

bool device_map_event(
    uint8_t* eventType,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2)
{
    if (!event_pending) return false;
    event_pending = false;

    if (eventType != nullptr) *eventType = SH_EVENT_RAIN_DETECTED;
    if (trigger != nullptr) *trigger = SH_TRIGGER_AUTO;
    if (param1 != nullptr) *param1 = event_regenstatus ? 1U : 0U;
    if (param2 != nullptr) *param2 = event_raw;
    return true;
}

uint64_t device_wake_candidates() {
#if BAT_SEN_ENABLE_GPIO_WAKE
    if (BAT_SEN_RAIN_SIGNAL_PIN < 0 || BAT_SEN_RAIN_SIGNAL_PIN >= 64) {
        return 0ULL;
    }
    return (1ULL << (uint8_t)BAT_SEN_RAIN_SIGNAL_PIN);
#else
    return 0ULL;
#endif
}
