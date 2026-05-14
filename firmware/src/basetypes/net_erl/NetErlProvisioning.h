// =============================================================================
// NetErlProvisioning.h – Provisioning-Config-Factory fuer NET-ERL
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/net_erl/NetErlProvisioning.h
//
// Datei-Funktion:
//   Factory-Konfiguration fuer das NodeProvisioning-Framework.
//   Definiert Setup-AP-Parameter (SSID-Passwort, Kanal), Storage-Namespaces
//   und stellt die makeConfig()-Factory-Funktion bereit.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Änderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>

namespace SmartHome {
namespace NetErlProvisioning {

// =============================================================================
// KONSTANTEN – Setup-AP und Storage
// =============================================================================

// Passwort fuer den Setup-AccessPoint (wird im Web-Provisioning abgefragt)
constexpr const char* SETUP_AP_PASSWORD = "net-erl-setup";

// Default-Storage-Namespace fuer Preferences (NVS-Partition)
constexpr const char* DEFAULT_STORAGE_NAMESPACE = "net_erl";

// Storage-Key fuer den NodeBasis-Datensatz (Version v1)
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";

// WLAN-Kanal des Setup-AccessPoints
constexpr int SETUP_AP_CHANNEL = 1;

// Verzoegerung zwischen Restart-Anforderung und tatsaechlichem Neustart (ms)
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

// =============================================================================
// FACTORY-FUNKTIONEN – NodeProvisioningConfig erzeugen
// =============================================================================

// makeConfig – Erstellt eine vollstaendige NodeProvisioning-Konfiguration
//   (mit explizitem Storage-Namespace).
//   Parameter: deviceId(const char*) – Geraete-ID (z.B. "net_erl_01")
//              storageNamespace(const char*) – NVS-Namespace (z.B. "net_erl")
//              defaultStatusSendIntervalS(uint32_t) – Default-Intervall STATE in s
//              defaultSensorSendIntervalS(uint32_t) – Default-Intervall Sensor in s
//              minSendIntervalS(uint32_t) – Minimal zulaessiges Intervall in s
//              maxSendIntervalS(uint32_t) – Maximal zulaessiges Intervall in s
//   Rückgabe: NodeProvisioningConfig (Struktur mit allen Setup-Parametern)
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

// makeConfig – Erstellt eine NodeProvisioning-Konfiguration mit Default-Namespace
//   (bequemere Variante – verwendet DEFAULT_STORAGE_NAMESPACE als NVS-Namespace).
//   Parameter: deviceId(const char*) – Geraete-ID
//              defaultStatusSendIntervalS(uint32_t) – Default STATE-Intervall
//              defaultSensorSendIntervalS(uint32_t) – Default Sensor-Intervall
//              minSendIntervalS(uint32_t) – Minimales Intervall
//              maxSendIntervalS(uint32_t) – Maximales Intervall
//   Rückgabe: NodeProvisioningConfig
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
