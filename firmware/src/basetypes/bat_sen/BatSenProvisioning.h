#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>

namespace SmartHome {
namespace BatSenProvisioning {

constexpr const char* SETUP_AP_PASSWORD = "bat-sen-setup";
constexpr const char* STORAGE_NAMESPACE = "bat_sen";
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";
constexpr int SETUP_AP_CHANNEL = 1;
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

inline ShNodeProvisioning::NodeProvisioningConfig makeConfig(
    const char* deviceId,
    uint32_t defaultWakeIntervalS,
    uint32_t defaultRxWindowMs,
    uint32_t minWakeIntervalS,
    uint32_t maxWakeIntervalS,
    uint32_t minRxWindowMs,
    uint32_t maxRxWindowMs) {
    ShNodeProvisioning::NodeProvisioningConfig config = {
        deviceId,
        SETUP_AP_PASSWORD,
        STORAGE_NAMESPACE,
        STORAGE_KEY_NODE_BASIS,
        defaultWakeIntervalS,
        defaultRxWindowMs,
        minWakeIntervalS,
        maxWakeIntervalS,
        RESTART_DELAY_MS,
        SETUP_AP_CHANNEL,
    };

    config.statusSendIntervalFieldName = "wake_interval_s";
    config.sensorSendIntervalFieldName = "rx_window_ms";
    config.statusSendIntervalLabel = "wake_interval_s";
    config.sensorSendIntervalLabel = "rx_window_ms";
    config.statusSendIntervalHint = "Zyklischer Wake- und Life-Sign-Takt in Sekunden.";
    config.sensorSendIntervalHint = "Empfangsfenster nach Wake/Event in Millisekunden.";
    config.minSensorSendIntervalS = minRxWindowMs;
    config.maxSensorSendIntervalS = maxRxWindowMs;
    return config;
}

}  // namespace BatSenProvisioning
}  // namespace SmartHome
