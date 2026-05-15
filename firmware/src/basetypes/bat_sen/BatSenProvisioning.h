// =============================================================================
// BatSenProvisioning.h – Provisioning-Config-Factory fuer BAT-SEN
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/bat_sen/BatSenProvisioning.h
//
// Datei-Funktion:
//   Factory-Konfiguration fuer das NodeProvisioning-Framework.
//   Definiert Setup-AP-Parameter, Storage-Namespaces und die
//   makeConfig()-Factory. BAT-SEN hat zwei Konfig-Felder:
//   wake_interval_s (Wake-Takt) und rx_window_ms (Empfangsfenster).
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
namespace BatSenProvisioning {

// Passwort fuer den Setup-AccessPoint
constexpr const char* SETUP_AP_PASSWORD = "bat-sen-setup";
// Storage-Namespace fuer Preferences
constexpr const char* STORAGE_NAMESPACE = "bat_sen";
// Storage-Key fuer NodeBasis-Datensatz
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";
// WLAN-Kanal des Setup-AP
constexpr int SETUP_AP_CHANNEL = 1;
// Verzoegerung bis zum Neustart (ms)
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

// makeConfig – Erstellt vollstaendige NodeProvisioning-Konfiguration
//   Parameter: deviceId – Geraete-ID
//              defaultWakeIntervalS – Default Wake-Takt in s
//              defaultRxWindowMs – Default RX-Fenster in ms
//              minWakeIntervalS, maxWakeIntervalS – Wake-Takt-Grenzen
//              minRxWindowMs, maxRxWindowMs – RX-Fenster-Grenzen
//   Rückgabe: NodeProvisioningConfig (mit benutzerdefinierten Feldnamen)
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

    // Umbenennung der Standard-Felder fuer Batterie-Geraete
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
