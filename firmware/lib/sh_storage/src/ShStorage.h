/*
====================================================================
 Projekt   : SmartHome ESP32
 Datei     : ShStorage.h
 Modul     : ShStorage
 Version   : 0.1.0
 Stand     : 2026-03-15

 Funktion:
 Gemeinsamer persistenter Speicher fuer Node-Setup und
 Basis-Provisionierung.

 Dieser Stand speichert bewusst nur das gemeinsame Fundament:
 - Master-MAC-Bindung
 - lokaler Anzeigename
 - einfache Grundwerte fuer spaetere Runtime-Konfiguration

 Nicht Teil dieses Moduls:
 - MQTT- oder Server-Konfiguration in Nodes
 - grosse per-Geraet-Sonderstrukturen
 - ausufernde Versionierungs- oder Migrationslogik
====================================================================
*/

#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <Protocol.h>

namespace SmartHome {
namespace ShStorage {

constexpr uint32_t SH_NODE_SETTINGS_MAGIC = 0x53484E50UL;
constexpr uint16_t SH_NODE_SETTINGS_VERSION = 2U;
constexpr uint32_t SH_NODE_SETTINGS_FLAG_MASTER_BOUND = 0x00000001UL;
constexpr uint32_t SH_NODE_SETTINGS_FLAG_AUTO_OFF_DELAY_SET = 0x00000002UL;
constexpr uint32_t SH_NODE_SETTINGS_FLAG_LIGHT_THRESHOLD_SET = 0x00000004UL;

constexpr uint32_t SH_STORED_REPORT_INTERVAL_MIN_S = 10UL;
constexpr uint32_t SH_STORED_REPORT_INTERVAL_MAX_S = 65535UL;
constexpr uint32_t SH_STORED_WAKE_INTERVAL_MIN_S = 10UL;
constexpr uint32_t SH_STORED_WAKE_INTERVAL_MAX_S = 604800UL;
constexpr uint32_t SH_STORED_AUTO_OFF_DELAY_MIN_S = 0UL;
constexpr uint32_t SH_STORED_AUTO_OFF_DELAY_MAX_S = 65535UL;
constexpr uint16_t SH_STORED_LIGHT_THRESHOLD_ON_MIN = 0U;
constexpr uint16_t SH_STORED_LIGHT_THRESHOLD_ON_MAX = 2000U;

static_assert(
    SH_STORED_REPORT_INTERVAL_MAX_S <= 65535UL,
    "Shared report interval must fit into protocol uint16 state fields.");

static_assert(
    SH_STORED_AUTO_OFF_DELAY_MAX_S <= 65535UL,
    "Shared auto-off delay must fit into protocol uint16 cfg/state fields.");

struct SharedNodeSettings {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t flags;
    uint8_t master_mac[6];
    uint8_t reserved_mac[2];
    char device_name[SH_DEVICE_NAME_LEN];
    uint32_t report_interval_s;
    uint32_t wake_interval_s;
    uint32_t auto_off_delay_s;
    uint16_t light_threshold_on;
    uint16_t reserved_cfg;
};

void makeDefaultSettings(
    SharedNodeSettings& settings,
    const char* defaultDeviceName,
    uint32_t defaultReportIntervalS,
    uint32_t defaultWakeIntervalS);

bool isSettingsStructValid(const SharedNodeSettings& settings);
bool hasStoredMasterMac(const SharedNodeSettings& settings);
bool hasStoredAutoOffDelaySeconds(const SharedNodeSettings& settings);
bool hasStoredLightThresholdOn(const SharedNodeSettings& settings);
bool isValidStoredReportIntervalSeconds(uint32_t reportIntervalS);
bool isValidStoredWakeIntervalSeconds(uint32_t wakeIntervalS);
bool isValidStoredAutoOffDelaySeconds(uint32_t autoOffDelayS);
bool isValidStoredLightThresholdOn(uint16_t lightThresholdOn);
bool setStoredMasterMac(SharedNodeSettings& settings, const uint8_t* masterMac);
void clearStoredMasterMac(SharedNodeSettings& settings);
void setStoredDeviceName(SharedNodeSettings& settings, const char* deviceName);
void setStoredReportIntervalSeconds(SharedNodeSettings& settings, uint32_t reportIntervalS);
void setStoredWakeIntervalSeconds(SharedNodeSettings& settings, uint32_t wakeIntervalS);
void setStoredAutoOffDelaySeconds(SharedNodeSettings& settings, uint32_t autoOffDelayS);
void clearStoredAutoOffDelaySeconds(SharedNodeSettings& settings);
void setStoredLightThresholdOn(SharedNodeSettings& settings, uint16_t lightThresholdOn);
void clearStoredLightThresholdOn(SharedNodeSettings& settings);
uint32_t effectiveReportIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultReportIntervalS);
uint32_t effectiveWakeIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultWakeIntervalS);
uint32_t effectiveAutoOffDelaySeconds(const SharedNodeSettings& settings, uint32_t defaultAutoOffDelayS);
uint16_t effectiveLightThresholdOn(const SharedNodeSettings& settings, uint16_t defaultLightThresholdOn);
bool usesDefaultReportIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultReportIntervalS);
bool usesDefaultWakeIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultWakeIntervalS);
bool usesDefaultAutoOffDelaySeconds(const SharedNodeSettings& settings, uint32_t defaultAutoOffDelayS);
bool usesDefaultLightThresholdOn(const SharedNodeSettings& settings, uint16_t defaultLightThresholdOn);
void normalizeComfortOverridesAgainstDefaults(
    SharedNodeSettings& settings,
    uint32_t defaultAutoOffDelayS,
    uint16_t defaultLightThresholdOn);
void normalizeBasisValuesAgainstDefaults(
    SharedNodeSettings& settings,
    uint32_t defaultReportIntervalS,
    uint32_t defaultWakeIntervalS,
    uint32_t defaultAutoOffDelayS,
    uint16_t defaultLightThresholdOn);
bool parseMacText(const char* text, uint8_t outMac[6]);
void formatMacText(const uint8_t* mac, char* buffer, size_t bufferSize);

class SharedNodeStorage {
  public:
    SharedNodeStorage() = default;

    bool load(
        SharedNodeSettings& settings,
        const char* defaultDeviceName,
        uint32_t defaultReportIntervalS,
        uint32_t defaultWakeIntervalS);

    bool save(const SharedNodeSettings& settings);
    bool factoryReset();
};

}  // namespace ShStorage
}  // namespace SmartHome
