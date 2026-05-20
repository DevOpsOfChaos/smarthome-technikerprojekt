// =============================================================================
// NetZrlProvisioning.h – Provisioning-Config-Factory fuer NET-ZRL
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_zrl_shutter_module/NetZrlProvisioning.h
//
// Datei-Funktion:
//   Factory-Konfiguration fuer das NodeProvisioning-Framework.
//   Definiert Setup-AP-Parameter (SSID-Passwort, Kanal), Storage-Namespaces
//   und die makeConfig()-Factory-Funktion fuer NET-ZRL-Geraete.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>

namespace SmartHome {
namespace NetZrlProvisioning {

// =============================================================================
// KONSTANTEN – Setup-AP und Storage
// =============================================================================

// Passwort fuer den Setup-AccessPoint
constexpr const char* SETUP_AP_PASSWORD = "net-zrl-setup";

// Storage-Namespace fuer Preferences (NVS-Partition)
constexpr const char* STORAGE_NAMESPACE = "net_zrl";

// Storage-Key fuer den NodeBasis-Datensatz (Version v1)
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";

// WLAN-Kanal des Setup-AccessPoints
constexpr int SETUP_AP_CHANNEL = 1;

// Verzoegerung zwischen Restart-Anforderung und tatsaechlichem Neustart (ms)
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

// =============================================================================
// FACTORY-FUNKTION – NodeProvisioningConfig erzeugen
// =============================================================================

// makeConfig – Erstellt eine vollstaendige NodeProvisioning-Konfiguration
//   Parameter: deviceId(const char*) – Geraete-ID (z.B. "NET-ZRL-001")
//              defaultStatusSendIntervalS(uint32_t) – Default STATE-Intervall in s
//              defaultSensorSendIntervalS(uint32_t) – Default Sensor-Intervall in s
//              minSendIntervalS(uint32_t) – Minimal zulaessiges Intervall
//              maxSendIntervalS(uint32_t) – Maximal zulaessiges Intervall
//   Rückgabe: NodeProvisioningConfig (Struktur mit allen Setup-Parametern)
inline ShNodeProvisioning::NodeProvisioningConfig makeConfig(
    const char* deviceId,
    uint32_t defaultStatusSendIntervalS,
    uint32_t defaultSensorSendIntervalS,
    uint32_t minSendIntervalS,
    uint32_t maxSendIntervalS) {
    return {
        deviceId,
        SETUP_AP_PASSWORD,
        STORAGE_NAMESPACE,
        STORAGE_KEY_NODE_BASIS,
        defaultStatusSendIntervalS,
        defaultSensorSendIntervalS,
        minSendIntervalS,
        maxSendIntervalS,
        RESTART_DELAY_MS,
        SETUP_AP_CHANNEL,
    };
}

}  // namespace NetZrlProvisioning
}  // namespace SmartHome
