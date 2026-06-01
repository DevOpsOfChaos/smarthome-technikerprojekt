// =============================================================================
// DeviceConfig.h – Geraetekonfiguration fuer BAT-SEN Basistyp
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/bat_sen/DeviceConfig.h
//
// Datei-Funktion:
//   Default-Konfiguration fuer BAT-SEN-Batteriesensoren. Definiert
//   Geraete-Identitaet, Battery-Profile (Spannungsbereiche fuer
//   CR2032, AA, AAA, LiIon), Wake-Intervalle, RX-Fenster und
//   ADC-Konfiguration. Enthaelt static_asserts zur Compilezeit-
//   Validierung aller Grenzen.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include <stdint.h>

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

// =============================================================================
// GERAETE-IDENTITAET – ID, Name, Variante (pro Device ueberschreibbar)
// =============================================================================

#ifndef BAT_SEN_DEVICE_ID
#define BAT_SEN_DEVICE_ID "BAT-SEN-001"
#endif

#ifndef BAT_SEN_DEVICE_NAME
#define BAT_SEN_DEVICE_NAME "BAT-SEN Base"
#endif

#ifndef BAT_SEN_FW_VARIANT
#define BAT_SEN_FW_VARIANT "bat_sen_base"
#endif

#ifndef BAT_SEN_DEVICE_CAPS
#define BAT_SEN_DEVICE_CAPS SH_CAP_BATTERY
#endif

// =============================================================================
// REPORTING – Reporting-Modus (Batterie: sleep_event)
// =============================================================================

#ifndef BAT_SEN_REPORTING_MODE
#define BAT_SEN_REPORTING_MODE SH_REPORTING_SLEEP_EVENT
#endif

// =============================================================================
// FUNK – WLAN-Kanal
// =============================================================================

#ifndef BAT_SEN_WLAN_CHANNEL
#define BAT_SEN_WLAN_CHANNEL 6
#endif

// =============================================================================
// DEBUG – Serielle Ausgaben
// =============================================================================

#ifndef BAT_SEN_DEBUG_ENABLED
#define BAT_SEN_DEBUG_ENABLED 1
#endif

// =============================================================================
// TIMING – Intervalle fuer HELLO, Discovery, Wake, RX, Loop
// =============================================================================

// HELLO-Wiederholung bei Master-Suche (ms)
#ifndef BAT_SEN_HELLO_RETRY_INTERVAL_MS
#define BAT_SEN_HELLO_RETRY_INTERVAL_MS 5000UL
#endif

// Discovery-Fenster: Zeit nach Boot in der auf HELLO_ACK gewartet wird (ms)
#ifndef BAT_SEN_DISCOVERY_WINDOW_MS
#define BAT_SEN_DISCOVERY_WINDOW_MS 25000UL
#endif

// Wake-Intervall: Wie oft das Geraet aufwacht (Sekunden, Default: 15min)
#ifndef BAT_SEN_DEFAULT_WAKE_INTERVAL_S
#define BAT_SEN_DEFAULT_WAKE_INTERVAL_S 900U
#endif

// RX-Fenster: Wie lange nach Wake auf eingehende Nachrichten gewartet wird (ms)
#ifndef BAT_SEN_DEFAULT_RX_WINDOW_MS
#define BAT_SEN_DEFAULT_RX_WINDOW_MS 800U
#endif

// Wake-Intervall-Grenzen
#ifndef BAT_SEN_MIN_WAKE_INTERVAL_S
#define BAT_SEN_MIN_WAKE_INTERVAL_S 30U
#endif

#ifndef BAT_SEN_MAX_WAKE_INTERVAL_S
#define BAT_SEN_MAX_WAKE_INTERVAL_S 65535U
#endif

// RX-Fenster-Grenzen
#ifndef BAT_SEN_MIN_RX_WINDOW_MS
#define BAT_SEN_MIN_RX_WINDOW_MS 500U
#endif

#ifndef BAT_SEN_MAX_RX_WINDOW_MS
#define BAT_SEN_MAX_RX_WINDOW_MS 60000U
#endif

// Loop-Ausfuehrungsintervall (ms)
#ifndef BAT_SEN_LOOP_INTERVAL_MS
#define BAT_SEN_LOOP_INTERVAL_MS 25UL
#endif

// =============================================================================
// BATTERIE – Sampling und Hysterese
// =============================================================================

// Abstand zwischen Batterie-Messungen (ms)
#ifndef BAT_SEN_BATTERY_SAMPLE_INTERVAL_MS
#define BAT_SEN_BATTERY_SAMPLE_INTERVAL_MS 5000UL
#endif

// Hysterese: Mindestaenderung in mV um neuen STATE auszuloesen
#ifndef BAT_SEN_BATTERY_STATE_DELTA_MV
#define BAT_SEN_BATTERY_STATE_DELTA_MV 10U
#endif

// Hysterese: Mindestaenderung in Prozent um neuen STATE auszuloesen
#ifndef BAT_SEN_BATTERY_STATE_DELTA_PCT
#define BAT_SEN_BATTERY_STATE_DELTA_PCT 1U
#endif

// =============================================================================
// BATTERY-PROFILE – Spannungsbereiche fuer verschiedene Batterietypen
// =============================================================================

#define BAT_PROFILE_CR2032 1U   // CR2032 Knopfzelle (2.2V-3.0V)
#define BAT_PROFILE_2X_AA   2U  // 2x AA in Reihe (2.0V-3.2V)
#define BAT_PROFILE_3X_AA   3U  // 3x AA in Reihe (3.0V-4.8V)
#define BAT_PROFILE_2X_AAA  4U  // 2x AAA in Reihe (2.0V-3.2V)
#define BAT_PROFILE_3X_AAA  5U  // 3x AAA in Reihe (3.0V-4.8V)
#define BAT_PROFILE_LIION_1S 6U // 1S LiIon/LiPo (3.0V-4.2V)

#ifndef BAT_SEN_BATTERY_PROFILE
#define BAT_SEN_BATTERY_PROFILE BAT_PROFILE_2X_AA
#endif

// Spannungsteiler (num/den): z.B. 2/1 = keine Teilung, 4/1 = Viertel
#ifndef BAT_SEN_BATTERY_DIVIDER_NUM
#define BAT_SEN_BATTERY_DIVIDER_NUM 2U
#endif

#ifndef BAT_SEN_BATTERY_DIVIDER_DEN
#define BAT_SEN_BATTERY_DIVIDER_DEN 1U
#endif

// Kalibrierung der realen Messkette nach erstem BAT-SEN-Test:
// Multimeter 2900mV, Firmware 2690mV -> Faktor ca. 1.078.
#ifndef BAT_SEN_BATTERY_CALIBRATION_NUM
#define BAT_SEN_BATTERY_CALIBRATION_NUM 1078U
#endif

#ifndef BAT_SEN_BATTERY_CALIBRATION_DEN
#define BAT_SEN_BATTERY_CALIBRATION_DEN 1000U
#endif

// ADC-Mittelung: Anzahl Samples pro Messung
#ifndef BAT_SEN_BATTERY_ADC_SAMPLE_COUNT
#define BAT_SEN_BATTERY_ADC_SAMPLE_COUNT 4U
#endif

// =============================================================================
// FEATURE-FLAGS – Deep Sleep, ADC, GPIO-Wake
// =============================================================================

#ifndef BAT_SEN_ENABLE_DEEP_SLEEP
#define BAT_SEN_ENABLE_DEEP_SLEEP 1
#endif

#ifndef BAT_SEN_ENABLE_ADC_BATTERY
#define BAT_SEN_ENABLE_ADC_BATTERY 1
#endif

#ifndef BAT_SEN_ENABLE_GPIO_WAKE
#define BAT_SEN_ENABLE_GPIO_WAKE 0
#endif

#ifndef BAT_SEN_ENABLE_STAY_AWAKE_TOGGLE
#define BAT_SEN_ENABLE_STAY_AWAKE_TOGGLE 1
#endif

// =============================================================================
// ABGELEITETE KONSTANTEN – constexpr-Werte aus #defines
// =============================================================================

constexpr char DEVICE_ID[] = BAT_SEN_DEVICE_ID;
constexpr char DEVICE_NAME[] = BAT_SEN_DEVICE_NAME;
constexpr char FW_VARIANT[] = BAT_SEN_FW_VARIANT;
constexpr bool DEVICE_DEBUG_AKTIV = BAT_SEN_DEBUG_ENABLED != 0;
constexpr uint16_t DEVICE_CAPS = (uint16_t)BAT_SEN_DEVICE_CAPS;

constexpr uint8_t DEVICE_META_SCHEMA_VERSION = SH_META_SCHEMA_VERSION_CURRENT;
constexpr uint8_t DEVICE_CONTROL_MODE = SH_CONTROL_MODE_NONE;
constexpr uint8_t DEVICE_CONFIG_PROFILE = SH_PROFILE_NONE;
constexpr uint8_t DEVICE_REPORTING_MODE = BAT_SEN_REPORTING_MODE;

constexpr int WLAN_KANAL = BAT_SEN_WLAN_CHANNEL;
constexpr unsigned long HELLO_RETRY_INTERVAL_MS = BAT_SEN_HELLO_RETRY_INTERVAL_MS;
constexpr unsigned long DISCOVERY_WINDOW_MS = BAT_SEN_DISCOVERY_WINDOW_MS;
constexpr unsigned long DEFAULT_WAKE_INTERVAL_S = BAT_SEN_DEFAULT_WAKE_INTERVAL_S;
constexpr unsigned long DEFAULT_RX_WINDOW_MS = BAT_SEN_DEFAULT_RX_WINDOW_MS;
constexpr unsigned long MIN_WAKE_INTERVAL_S = BAT_SEN_MIN_WAKE_INTERVAL_S;
constexpr unsigned long MAX_WAKE_INTERVAL_S = BAT_SEN_MAX_WAKE_INTERVAL_S;
constexpr unsigned long MIN_RX_WINDOW_MS = BAT_SEN_MIN_RX_WINDOW_MS;
constexpr unsigned long MAX_RX_WINDOW_MS = BAT_SEN_MAX_RX_WINDOW_MS;
constexpr unsigned long LOOP_INTERVAL_MS = BAT_SEN_LOOP_INTERVAL_MS;
constexpr unsigned long BATTERY_SAMPLE_INTERVAL_MS = BAT_SEN_BATTERY_SAMPLE_INTERVAL_MS;
constexpr uint16_t BATTERY_STATE_DELTA_MV = BAT_SEN_BATTERY_STATE_DELTA_MV;
constexpr uint8_t BATTERY_STATE_DELTA_PCT = BAT_SEN_BATTERY_STATE_DELTA_PCT;

// BatteryProfile – Spannungsbereich (leer/voll) fuer einen Batterietyp
struct BatteryProfile {
    uint16_t emptyMv;  // Spannung bei 0% (mV)
    uint16_t fullMv;   // Spannung bei 100% (mV)
};

// batteryProfileFor – Liefert Spannungsbereich fuer ein Batterie-Profil
constexpr BatteryProfile batteryProfileFor(uint8_t profile) {
    return profile == BAT_PROFILE_CR2032
               ? BatteryProfile{2200U, 3000U}
               : profile == BAT_PROFILE_2X_AA
                     ? BatteryProfile{2000U, 3200U}
                     : profile == BAT_PROFILE_3X_AA
                           ? BatteryProfile{3000U, 4800U}
                           : profile == BAT_PROFILE_2X_AAA
                                 ? BatteryProfile{2000U, 3200U}
                                 : profile == BAT_PROFILE_3X_AAA
                                       ? BatteryProfile{3000U, 4800U}
                                       : BatteryProfile{3000U, 4200U};  // Default: LiIon 1S
}

constexpr BatteryProfile SELECTED_BATTERY_PROFILE = batteryProfileFor(BAT_SEN_BATTERY_PROFILE);
constexpr uint16_t BATTERY_EMPTY_MV = SELECTED_BATTERY_PROFILE.emptyMv;
constexpr uint16_t BATTERY_FULL_MV = SELECTED_BATTERY_PROFILE.fullMv;
constexpr uint16_t BATTERY_DIVIDER_NUM = BAT_SEN_BATTERY_DIVIDER_NUM;
constexpr uint16_t BATTERY_DIVIDER_DEN = BAT_SEN_BATTERY_DIVIDER_DEN;
constexpr uint16_t BATTERY_CALIBRATION_NUM = BAT_SEN_BATTERY_CALIBRATION_NUM;
constexpr uint16_t BATTERY_CALIBRATION_DEN = BAT_SEN_BATTERY_CALIBRATION_DEN;
constexpr uint8_t BATTERY_ADC_SAMPLE_COUNT = BAT_SEN_BATTERY_ADC_SAMPLE_COUNT;

constexpr bool DEEP_SLEEP_AKTIV = BAT_SEN_ENABLE_DEEP_SLEEP != 0;
constexpr bool BATTERY_ADC_AKTIV = BAT_SEN_ENABLE_ADC_BATTERY != 0;
constexpr bool GPIO_WAKE_AKTIV = BAT_SEN_ENABLE_GPIO_WAKE != 0;
constexpr bool STAY_AWAKE_TOGGLE_AKTIV = BAT_SEN_ENABLE_STAY_AWAKE_TOGGLE != 0;

// =============================================================================
// COMPILEZEIT-VALIDIERUNG – static_asserts fuer Profile und Grenzen
// =============================================================================

static_assert(BATTERY_DIVIDER_DEN > 0U, "BAT_SEN_BATTERY_DIVIDER_DEN darf nicht 0 sein.");
static_assert(BATTERY_CALIBRATION_DEN > 0U, "BAT_SEN_BATTERY_CALIBRATION_DEN darf nicht 0 sein.");
static_assert(BATTERY_ADC_SAMPLE_COUNT > 0U, "BAT_SEN_BATTERY_ADC_SAMPLE_COUNT darf nicht 0 sein.");
static_assert(BATTERY_FULL_MV > BATTERY_EMPTY_MV, "BAT_SEN_BATTERY_PROFILE liefert ungueltige Spannungsgrenzen.");
static_assert(
    BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_CR2032 ||
        BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_2X_AA ||
        BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_3X_AA ||
        BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_2X_AAA ||
        BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_3X_AAA ||
        BAT_SEN_BATTERY_PROFILE == BAT_PROFILE_LIION_1S,
    "BAT_SEN_BATTERY_PROFILE muss ein bekanntes BAT_PROFILE_* sein.");
static_assert(DEFAULT_WAKE_INTERVAL_S >= MIN_WAKE_INTERVAL_S, "BAT_SEN_DEFAULT_WAKE_INTERVAL_S unter Minimum.");
static_assert(DEFAULT_WAKE_INTERVAL_S <= MAX_WAKE_INTERVAL_S, "BAT_SEN_DEFAULT_WAKE_INTERVAL_S ueber Maximum.");
static_assert(DEFAULT_RX_WINDOW_MS >= MIN_RX_WINDOW_MS, "BAT_SEN_DEFAULT_RX_WINDOW_MS unter Minimum.");
static_assert(DEFAULT_RX_WINDOW_MS <= MAX_RX_WINDOW_MS, "BAT_SEN_DEFAULT_RX_WINDOW_MS ueber Maximum.");
