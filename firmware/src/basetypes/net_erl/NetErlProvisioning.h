#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>

namespace SmartHome {
namespace NetErlProvisioning {

constexpr const char* SETUP_AP_PASSWORD = "net-erl-setup";
constexpr const char* DEFAULT_STORAGE_NAMESPACE = "net_erl";
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";
constexpr int SETUP_AP_CHANNEL = 1;
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

inline ShNodeProvisioning::NodeProvisioningConfig makeConfig(
    const char* deviceId,
    const char* storageNamespace,
    uint32_t defaultStatusSendIntervalS,
    uint32_t defaultSensorSendIntervalS,
    uint32_t minSendIntervalS,
    uint32_t maxSendIntervalS) {
    return {
        deviceId,
        SETUP_AP_PASSWORD,
        storageNamespace,
        STORAGE_KEY_NODE_BASIS,
        defaultStatusSendIntervalS,
        defaultSensorSendIntervalS,
        minSendIntervalS,
        maxSendIntervalS,
        RESTART_DELAY_MS,
        SETUP_AP_CHANNEL,
    };
}

inline ShNodeProvisioning::NodeProvisioningConfig makeConfig(
    const char* deviceId,
    uint32_t defaultStatusSendIntervalS,
    uint32_t defaultSensorSendIntervalS,
    uint32_t minSendIntervalS,
    uint32_t maxSendIntervalS) {
    return makeConfig(
        deviceId,
        DEFAULT_STORAGE_NAMESPACE,
        defaultStatusSendIntervalS,
        defaultSensorSendIntervalS,
        minSendIntervalS,
        maxSendIntervalS);
}

}  // namespace NetErlProvisioning
}  // namespace SmartHome
